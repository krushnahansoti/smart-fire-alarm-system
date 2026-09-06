// ============================================================
// config.example.h
// Copy this file to "config.h" and fill in your own values.
// config.h is git-ignored so your secrets never get committed.
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// ------------------ WiFi ------------------
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ------------------ Local MQTT Broker ------------------
const char* mqtt_server = "YOUR_MQTT_BROKER_IP"; // e.g. "192.168.1.50"

// ------------------ Telegram Bot ------------------
// Create a bot via @BotFather on Telegram to get a token.
// Message @userinfobot to get your chat ID.
const char* telegramToken = "YOUR_TELEGRAM_BOT_TOKEN";
const char* chatId        = "YOUR_TELEGRAM_CHAT_ID";

// ------------------ MacroDroid Cloud Trigger ------------------
// Create a "Webhook (URL)" trigger in MacroDroid to get this URL.
const char* macroDroidCloudURL = "https://trigger.macrodroid.com/YOUR_WEBHOOK_ID/fire_alert";

#endif
