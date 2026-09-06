# IoT-Based Smart Fire Alarm System

An ESP32-based fire detection system that combines gas, flame, and environmental sensing with real-time alerts via Telegram, MacroDroid (mobile), and a Node-RED dashboard — plus a local web interface for manual testing.
**Author:** Krushna Hansoti

## Features

- 🔥 Multi-sensor fire detection — MQ2 gas/smoke sensor + flame sensor + DHT11 temperature/humidity
- 📟 Local OLED status display
- 🔊 Immediate local alarm — buzzer + LED
- 📱 Mobile alerts — Telegram bot messages + MacroDroid webhook (ringtone, vibration, popup)
- 🌐 Web interface — trigger/test alarm and check status from a browser
- 📊 Node-RED dashboard — live gauges for gas level, temperature, humidity, and alarm state
- 📡 MQTT publishing for integration with other systems

## Hardware

| Component      | ESP32 Pin | Connection Type |
|----------------|-----------|------------------|
| MQ2 Sensor     | GPIO 34   | Analog Input     |
| Flame Sensor   | GPIO 33   | Digital Input    |
| DHT11 Sensor   | GPIO 25   | Digital Input    |
| Buzzer         | GPIO 26   | PWM Output       |
| LED            | GPIO 27   | Digital Output   |
| OLED Display   | SDA/SCL   | I2C              |

See `docs/circuit_diagram.png` for the full wiring diagram.

## Software / Services used

- Arduino IDE (ESP32 board package)
- MacroDroid (mobile automation)
- Telegram Bot API
- Node-RED (dashboard)
- Mosquitto (local MQTT broker)

## Getting started

### 1. Clone and open the sketch

```bash
git clone https://github.com/<your-username>/smart-fire-alarm-system.git
cd smart-fire-alarm-system/firmware
```

Open `fire_alarm.ino` in the Arduino IDE.

### 2. Set up your secrets

Copy the example config and fill in your own values — **this file is git-ignored so it never gets committed**:

```bash
cp config.example.h config.h
```

Edit `config.h` with:
- Your WiFi SSID/password
- Your local MQTT broker IP (e.g. a Mosquitto instance on your LAN)
- A Telegram bot token (create one via [@BotFather](https://t.me/BotFather)) and your chat ID (via [@userinfobot](https://t.me/userinfobot))
- A MacroDroid Webhook (URL) trigger link

### 3. Install required libraries

Via Arduino Library Manager:
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `DHT sensor library` (Adafruit)
- `PubSubClient`
- `ArduinoJson`

(WiFi, HTTPClient, WebServer are bundled with the ESP32 board package.)

### 4. Flash and run

Select your ESP32 board and port, then upload. On first boot the sketch calibrates the MQ2 sensor for ~10 seconds — keep it in clean air during this step.

### 5. (Optional) Node-RED dashboard

Import `node-red/flows.json` into your Node-RED instance and point it at the same MQTT broker to get live gauges for gas, temperature, humidity, and alarm state.

## How it works

- Sensors are read every 2 seconds.
- **Gas alert:** MQ2 analog reading > 1000
- **Fire alert:** flame sensor reads LOW
- On alarm: local buzzer/LED fire immediately, a Telegram message is sent, and a MacroDroid webhook is triggered (phone ringtone + vibration + popup), with cooldowns (30s Telegram / 60s mobile) to avoid spamming.
- An "all clear" message is sent once the alarm condition resolves.

## Project report

The full write-up, including sensor specs, working principle, and test outputs, is in [`docs/Project_Report.pdf`](docs/Project_Report.pdf).

## License

MIT — see [LICENSE](LICENSE).
