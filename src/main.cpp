// ESP32-S3 + PCM1808 -> Wi-Fi -> браузер телефона (Web Audio)
// Схема: джек ТВ -> PCM1808 -> I2S -> ESP32-S3 -> WebSocket -> страница на телефоне
//
// Подключиться к Wi-Fi "TV-Audio" и открыть http://192.168.4.1

#include <Arduino.h>
#include <secret.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <driver/i2s.h>

// ---------- настройки ----------
// Домашняя сеть: плата подключается к ней сама при старте
static const char* STA_SSID = STA_SSID_VALUE;
static const char* STA_PASS = STA_PASS_VALUE;

// Запасной вариант: если к роутеру подключиться не вышло, плата поднимет свою сеть
static const char* AP_SSID = "TV-Audio";
static const char* AP_PASS = "12345678";   // минимум 8 символов

// Пины I2S (не трогаем 35-37 и 26-32: заняты PSRAM/флешем на N16R8)
static const int PIN_MCLK = 7;   // -> SCK/SCKI на PCM1808
static const int PIN_BCK  = 4;   // -> BCK
static const int PIN_WS   = 5;   // -> LRC/LRCK
static const int PIN_DIN  = 6;   // <- OUT/DOUT

static const uint32_t SAMPLE_RATE = 32000;   // Гц
static const int      FRAMES      = 512;     // кадров (L+R) за один пакет, ~16 мс
static const int      SHIFT       = 16;      // 16 = как есть; 15/14 = громче в 2/4 раза (с ограничением)
static const i2s_port_t I2S_PORT  = I2S_NUM_0;

// Шумоподавитель: если сигнал тише порога дольше GATE_HOLD_BLOCKS пакетов (~16 мс каждый),
// отправляется тишина. Порог смотри в мониторе порта на холостом ходу. 0 = выключен.
static const int GATE_THRESHOLD   = 200;   // из 32767
static const int GATE_HOLD_BLOCKS = 25;    // ~0,4 с

WebServer server(80);
WebSocketsServer ws(81);

static int32_t raw[FRAMES * 2];   // 32-битные сэмплы от PCM1808
static int16_t pcm[FRAMES * 2];   // 16-битные для отправки

// ---------- страница ----------
static const char PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TV Audio</title>
<style>
 body{font-family:sans-serif;background:#111;color:#eee;text-align:center;padding:2em}
 button{font-size:1.6em;padding:1em 2em;border:0;border-radius:1em;background:#2a7;color:#fff}
 #s{margin-top:1.5em;opacity:.7}
</style></head><body>
<h2>Звук с телевизора</h2>
<button id="b">Слушать</button>
<div id="s">остановлено</div>
<div style="margin-top:2em">
 Буфер (задержка): <b id="tv">0.25</b> с<br>
 <input id="t" type="range" min="0.04" max="1" step="0.01" value="0.25" style="width:80%;margin-top:.8em">
 <div style="opacity:.6;font-size:.85em">меньше = меньше задержка, но возможны щелчки</div>
</div>
<script>
const RATE=32000;                // должна совпадать с SAMPLE_RATE в прошивке
let TARGET=0.25;                 // запас буфера, сек (меняется ползунком)
let ctx=null, sock=null, next=0, q=[];
const t=document.getElementById('t'), tv=document.getElementById('tv');
t.oninput=()=>{
  TARGET=parseFloat(t.value);
  tv.textContent=TARGET.toFixed(2);
  q.forEach(x=>{try{x.stop()}catch(e){}});   // сбрасываем уже поставленное в очередь,
  q=[]; next=0;                              // чтобы новое значение применилось сразу
};
const b=document.getElementById('b'), s=document.getElementById('s');

function stop(){
  if(sock){sock.onclose=null;sock.close();sock=null;}
  if(ctx){ctx.close();ctx=null;}
  b.textContent='Слушать'; s.textContent='остановлено';
}

b.onclick=()=>{
  if(ctx){stop();return;}
  ctx=new (window.AudioContext||window.webkitAudioContext)();
  ctx.resume();
  next=0;
  sock=new WebSocket('ws://'+location.hostname+':81/');
  sock.binaryType='arraybuffer';
  sock.onopen=()=>{b.textContent='Стоп'; s.textContent='играет';};
  sock.onclose=()=>{stop(); s.textContent='соединение потеряно';};
  sock.onmessage=e=>{
    const i16=new Int16Array(e.data);
    const n=i16.length;
    const buf=ctx.createBuffer(1,n,RATE);
    const M=buf.getChannelData(0);
    for(let i=0;i<n;i++){M[i]=i16[i]/32768;}
    const src=ctx.createBufferSource();
    src.buffer=buf; src.connect(ctx.destination);
    const now=ctx.currentTime;
    if(next<now+0.02) next=now+TARGET;   // буфер опустел - накапливаем заново
    if(next>now+TARGET+0.3) return;      // слишком большое отставание - выбрасываем пакет
    src.start(next);
    q.push(src);
    src.onended=()=>{q=q.filter(x=>x!==src);};
    next+=n/RATE;
  };
};
</script></body></html>
)rawliteral";

// ---------- I2S ----------
static void setupI2S() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate          = SAMPLE_RATE;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 8;
  cfg.dma_buf_len          = 256;
  cfg.use_apll             = false;
  cfg.fixed_mclk           = SAMPLE_RATE * 256;   // PCM1808 в slave-режиме ждёт MCLK = 256*fs

  i2s_pin_config_t pins = {};
  pins.mck_io_num   = PIN_MCLK;
  pins.bck_io_num   = PIN_BCK;
  pins.ws_io_num    = PIN_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num  = PIN_DIN;

  esp_err_t e = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (e != ESP_OK) Serial.printf("i2s_driver_install: %d\n", e);
  e = i2s_set_pin(I2S_PORT, &pins);
  if (e != ESP_OK) Serial.printf("i2s_set_pin: %d\n", e);
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== TV audio streamer ===");

  setupI2S();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.printf("Подключаюсь к %s ", STA_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false);   // меньше задержка
    Serial.printf("Подключено. Открой на телефоне: http://%s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin("tvaudio")) {
      Serial.println("Или по имени: http://tvaudio.local");
    }
  } else {
    Serial.println("Роутер не найден, поднимаю свою сеть.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.setSleep(false);
    Serial.printf("Wi-Fi: %s, адрес: http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  }

  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.begin();
  ws.begin();
}

void loop() {
  server.handleClient();
  ws.loop();

  size_t got = 0;
  i2s_read(I2S_PORT, raw, sizeof(raw), &got, 100 / portTICK_PERIOD_MS);
  int n = got / sizeof(int32_t);   // всего сэмплов (L+R вперемешку)
  int frames = n / 2;              // кадров = моно-сэмплов
  if (frames <= 0) return;

  // Моно: среднее левого и правого каналов
  static float dcX = 0, dcY = 0;   // состояние фильтра постоянной составляющей
  static int holdBlocks = 0;       // сколько пакетов ещё держим шумоподавитель открытым

  int32_t peak = 0;
  for (int i = 0; i < frames; i++) {
    float x = (float)(((raw[2 * i] >> 1) + (raw[2 * i + 1] >> 1)) >> SHIFT);
    float y = x - dcX + 0.995f * dcY;   // убираем постоянную составляющую
    dcX = x;
    dcY = y;
    int32_t mix = (int32_t)y;
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    pcm[i] = (int16_t)mix;
    if (abs(mix) > peak) peak = abs(mix);
  }

  // Шумоподавитель
  if (GATE_THRESHOLD > 0) {
    if (peak >= GATE_THRESHOLD) holdBlocks = GATE_HOLD_BLOCKS;
    else if (holdBlocks > 0) holdBlocks--;
    if (holdBlocks == 0) memset(pcm, 0, frames * sizeof(int16_t));
  }

  if (ws.connectedClients() > 0) {
    ws.broadcastBIN((uint8_t*)pcm, frames * sizeof(int16_t));
  }

  // раз в секунду печатаем уровень сигнала - удобно для проверки подключения
  static uint32_t t = 0;
  static int32_t maxPeak = 0;
  if (peak > maxPeak) maxPeak = peak;
  if (millis() - t > 1000) {
    t = millis();
    Serial.printf("уровень: %ld / 32767, клиентов: %u\n", (long)maxPeak, ws.connectedClients());
    maxPeak = 0;
  }
}