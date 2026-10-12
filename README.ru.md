<div align="center">

# 📡 WIFI-TV-Emitter

**Слушай старый телевизор в Bluetooth-наушниках через браузер телефона**

ESP32-S3 &nbsp;•&nbsp; PCM1808 &nbsp;•&nbsp; Wi-Fi &nbsp;•&nbsp; Web Audio

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ready-orange?logo=platformio&logoColor=white)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Espressif-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Arduino](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)](https://github.com/espressif/arduino-esp32)
[![WebSocket](https://img.shields.io/badge/audio-WebSocket-4c8bf5)](https://developer.mozilla.org/ru/docs/Web/API/Web_Audio_API)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Stars](https://img.shields.io/github/stars/Artin-cell/WIFI-TV-Emitter?style=social)](https://github.com/Artin-cell/WIFI-TV-Emitter)

[🇬🇧 English](README.md) &nbsp;·&nbsp; 🇷🇺 **Русский**

</div>

---

<!-- Добавь скриншот веб-страницы и раскомментируй:
<p align="center"><img src="docs/screenshot.png" width="320" alt="Веб-страница"></p>
-->

## 📖 Оглавление

- [✨ Возможности](#-возможности)
- [🧩 Как это работает](#-как-это-работает)
- [🔌 Железо и подключение](#-железо-и-подключение)
- [🚀 Установка](#-установка)
- [🎧 Использование](#-использование)
- [🔧 Настройка](#-настройка)
- [🚧 Ограничения](#-ограничения)
- [🧰 Устранение неполадок](#-устранение-неполадок)
- [🔗 Ссылки](#-ссылки)
- [📄 Лицензия](#-лицензия)

## ✨ Возможности

- 🎙️ Моно-поток, 16 бит, 32 кГц (около 64 КБ/с)
- 📱 На телефон ничего ставить не нужно: страницу отдаёт сама плата
- 🎚️ Два ползунка, меняются на лету:
  - **Буфер (задержка):** 0,1–1,0 с. Меньше — меньше задержка, но возможны щелчки.
  - **Порог шумодава:** 0–1500 (0 = выключен). Если сигнал ниже порога, отправляется тишина, и шум с треском на паузах не слышен.
- 🔧 Фильтр постоянной составляющей
- 📶 Сам подключается к домашнему Wi-Fi, а если роутер не найден, поднимает свою точку доступа
- 🌐 Доступен по адресу `http://tvaudio.local` (mDNS)

## 🧩 Как это работает

```mermaid
flowchart LR
    TV["📺 Телевизор<br/>джек 3,5 мм"] --> ADC["🎚️ PCM1808<br/>I2S АЦП"]
    ADC --> ESP["🧠 ESP32-S3"]
    ESP -- "Wi-Fi · WebSocket" --> PH["📱 Браузер телефона<br/>Web Audio"]
    PH -- Bluetooth --> HP["🎧 Наушники"]
```

ESP32-S3 оцифровывает аналоговый сигнал через PCM1808 по I2S и отправляет его в браузер по WebSocket. Страница воспроизводит его через Web Audio API.


> **Почему не подключить наушники напрямую?** Bluetooth-наушники работают по Classic Bluetooth (A2DP), а у ESP32-S3 есть только Bluetooth LE, поэтому передать на них звук он не может. Роль моста играет телефон. Обычный ESP32 (WROOM/WROVER) умеет A2DP напрямую, например с библиотекой [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP), но этот проект работает не так.

## 🔌 Железо и подключение

**Что нужно:**

- 🧠 Плата на ESP32-S3 (проверено на плате типа ESP32-S3-DevKitC-1)
- 🎚️ Модуль [PCM1808](https://aliexpress.ru/item/1005012718811058.html?shpMethod=CAINIAO_SUPER_ECONOMY&sku_id=12000059136057404&spm=a2g2w.productlist.search_results.0.6d114a68umo8Sn) (I2S АЦП)
- 🔊 Кабель или гнездо 3,5 мм для выхода наушников ТВ

Названия пинов у разных модулей PCM1808 отличаются, сверяйся с маркировкой на своей плате.

| | PCM1808 | ESP32-S3 / другое |
|---|---|---|
| ⚡ Питание | +5V (проверь что твоя плата именно отдает 5V на пине, а не только принимает, ниже фото контакта, от куда можно взять 5V)| 5V |
| ⚡ Питание | 3.3 | 3V3 |
| ⚡ Питание | GND | GND |
| 🧷 Режим | MD0, MD1, FMT | GND (ведомый режим, формат I2S) |
| 🕐 I2S | SCK (MCLK) | GPIO 7 |
| 🕐 I2S | BCK | GPIO 4 |
| 🕐 I2S | LRC (LRCK) | GPIO 5 |
| 🕐 I2S | OUT (DOUT) | GPIO 6 |
| 🔊 Вход | LIN | кончик джека (левый канал) |
| 🔊 Вход | RIN | среднее кольцо джека (правый канал) |
| 🔊 Вход | значок «земля» | гильза джека |

![5V](https://github.com/Artin-cell/WIFI-TV-Emitter/blob/main/media/photo_2026-10-12_13-17-47.jpg?raw=true)

Пины задаются в начале `src/main.cpp`. На платах, где GPIO 26–37 заняты под PSRAM или флеш, эти пины не используй.

## 🚀 Установка

1. Установи [PlatformIO](https://platformio.org/).
2. Склонируй репозиторий:
   ```bash
   git clone https://github.com/Artin-cell/WIFI-TV-Emitter.git
   cd WIFI-TV-Emitter
   ```
3. Скопируй `src/secret_example.h` в `src/secret.h` и впиши данные своего Wi-Fi:
   ```cpp
   #define STA_SSID_VALUE "имя_сети"
   #define STA_PASS_VALUE "пароль"
   ```
   > [!TIP]
   > `secret.h` записан в `.gitignore`, поэтому пароль в репозиторий не попадёт.
4. Собери и прошей:
   ```bash
   pio run -t upload
   ```
5. Подключи PCM1808 к джеку телевизора (начни с небольшой громкости ТВ).

## 🎧 Использование

1. Подключи телефон к той же Wi-Fi сети.
2. Открой в браузере `http://tvaudio.local`. На Android это имя может не открыться, тогда используй IP платы из списка клиентов роутера.
3. Нажми **Слушать** и надень Bluetooth-наушники.

Если за 15 секунд роутер не найден, плата создаст свою сеть `TV-Audio` (пароль `12345678`). Адрес: `http://192.168.4.1`.

## 🔧 Настройка

Константы в начале `src/main.cpp`:

| Константа | Значение |
|---|---|
| `SAMPLE_RATE` | Частота дискретизации. Должна совпадать с `RATE` в скрипте страницы. |
| `FRAMES` | Кадров в пакете (2048 ≈ 64 мс). Больше — реже пакеты, легче слабому телефону, чуть больше задержка. |
| `SHIFT` | 16 = как есть; 15 или 14 = громче в 2 или 4 раза (с ограничением от перегруза). |
| `GATE_DEFAULT` | Начальный порог шумодава. |
| `GATE_HOLD_BLOCKS` | Как долго шумодав остаётся открытым после последнего громкого пакета. |
| `AP_SSID`, `AP_PASS` | Запасная точка доступа. |

## 🚧 Ограничения

- ⏱️ **Задержка** примерно 0,3–0,5 с плюс задержка Bluetooth на телефоне. Для новостей и подкастов нормально, а для фильмов будет рассинхрон звука и губ.
- 📵 Держи экран телефона включённым: некоторые браузеры останавливают звук в фоне или при блокировке экрана.
- 🔊 Выход наушников ТВ может быть слишком громким для входа PCM1808. Если звук хрипит, уменьши громкость на ТВ.

## 🧰 Устранение неполадок

<details>
<summary>🔇 Нет звука, уровень всегда ноль</summary>

Проверь, что MD0, MD1 и FMT подтянуты к GND, что модуль запитан (5 В и 3,3 В), и что SCK, BCK, LRC и OUT стоят на нужных GPIO. Провод SCK лучше сделать коротким.

</details>

<details>
<summary>🔥 Треск на холостом ходу</summary>

Замкни LIN и RIN на GND. Если шум пропал, его ловят входные провода, либо он идёт от ТВ или петли заземления. Попробуй питание от повербанка, укороти и скрути входные провода, поставь конденсаторы (100 мкФ + 100 нФ) на питание рядом с модулем. Остатки спрячет ползунок шумодава.

</details>

<details>
<summary>🐌 Рывки на слабом телефоне</summary>

Подними ползунок буфера, держи экран включённым и отключи экономию заряда для браузера.

</details>

<details>
<summary>🌐 Страница не открывается на Android</summary>

Используй IP вместо `tvaudio.local` или отключи мобильный интернет.

</details>

## 🔗 Ссылки

- [PlatformIO](https://platformio.org/)
- [arduino-esp32](https://github.com/espressif/arduino-esp32)
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (Links2004)
- [Web Audio API (MDN)](https://developer.mozilla.org/ru/docs/Web/API/Web_Audio_API)
- [PCM1808 (Texas Instruments)](https://www.ti.com/product/PCM1808)

## 📄 Лицензия

Проект распространяется по лицензии [MIT](LICENSE).
