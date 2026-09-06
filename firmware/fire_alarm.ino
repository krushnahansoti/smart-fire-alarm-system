#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"   // <-- your local secrets (WiFi, MQTT, Telegram, MacroDroid)

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

// ------------------ Pins ------------------
#define MQ2_PIN 34
#define FLAME_PIN 33
#define DHT_PIN 25
#define BUZZER_PIN 26
#define LED_PIN 27

// ------------------ PWM for Buzzer ------------------
#define PWM_CHANNEL 0
#define PWM_FREQ 2000
#define PWM_RESOLUTION 8

// ------------------ OLED Setup ------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ------------------ DHT11 ------------------
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ------------------ MQ2 ------------------
float RL = 5.0;
float Ro = 10.0;
#define CALIBRATION_TIME 10000
#define CLEAN_AIR_RATIO 9.8
#define MQ2_THRESHOLD 1000
#define FLAME_TRIGGER LOW

// ------------------ Timing & State ------------------
unsigned long previousMillis = 0;
const long interval = 2000;
bool lastAlarmState = false;
unsigned long lastMobileAlert = 0;
unsigned long lastTelegramAlert = 0;
const long mobileCooldown = 60000;   // 1 minute between mobile alerts
const long telegramCooldown = 30000; // 30 seconds between Telegram

void setup() {
  Serial.begin(115200);

  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);

  ledcWrite(PWM_CHANNEL, 0);
  digitalWrite(LED_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  dht.begin();

  WiFi.begin(ssid, password);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  setupHTTPServer();
  calibrateMQ2();

  sendTelegramMessage("Fire Alarm System Started!\nIP: " +
                       WiFi.localIP().toString() + "\nMacroDroid Cloud Alerts Ready!");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("System Ready!");
  display.setCursor(0, 16);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.setCursor(0, 32);
  display.println("Cloud Alerts: ON");
  display.display();

  Serial.println("System fully initialized!");
}

void calibrateMQ2() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Calibrating MQ2...");
  display.display();

  unsigned long startTime = millis();
  float val = 0;
  int count = 0;

  while (millis() - startTime < CALIBRATION_TIME) {
    val += analogRead(MQ2_PIN);
    count++;

    if (count % 20 == 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Calibrating MQ2...");
      display.setCursor(0, 16);
      display.print("Time left: ");
      display.print((CALIBRATION_TIME - (millis() - startTime)) / 1000);
      display.println("s");
      display.display();
    }
    delay(50);
  }

  val = val / count;
  float Vrl = val * 3.3 / 4095.0;
  float Rs = (5.0 - Vrl) * RL / Vrl;
  Ro = Rs / CLEAN_AIR_RATIO;

  Serial.print("MQ2 Ro: ");
  Serial.println(Ro);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("MQ2 Calibrated!");
  display.print("Ro: ");
  display.println(Ro);
  display.display();
  delay(2000);
}

// ================== HTTP SERVER FOR MOBILE ==================
void setupHTTPServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<html><body style='font-family: Arial; text-align: center;'>";
    html += "<h1>Fire Alarm System</h1>";
    html += "<p>Status: <strong>Active</strong></p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Cloud Alerts: <strong style='color: green;'>ENABLED</strong></p>";
    html += "<button onclick=\"fetch('/test-alarm', {method: 'POST'})\" "
            "style='padding: 15px 30px; font-size: 18px; background: red; color: white; border: none; border-radius: 10px; margin: 5px;'>TEST ALARM</button>";
    html += "<br>";
    html += "<button onclick=\"fetch('/alarm', {method: 'POST'})\" style='padding: 15px 30px; font-size: 18px; background: orange; color: white; border: none; border-radius: 10px; margin: 5px;'>TRIGGER ALARM (API)</button>";
    html += "<br>";
    html += "<button onclick=\"fetch('/status')\" style='padding: 15px 30px; font-size: 18px; background: blue; color: white; border: none; border-radius: 10px; margin: 5px;'>GET STATUS</button>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/alarm", HTTP_POST, []() {
    Serial.println("ALARM triggered via POST /alarm endpoint!");

    int mq2ADC = analogRead(MQ2_PIN);
    int flameState = digitalRead(FLAME_PIN);
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (isnan(temp)) temp = 0;
    if (isnan(hum)) hum = 0;

    triggerAlarmOutputs(true);
    triggerMobileAlert(mq2ADC, flameState, temp, hum);

    String response = "{";
    response += "\"status\":\"success\",";
    response += "\"message\":\"Alarm triggered via API - Cloud alert sent\",";
    response += "\"mq2\":" + String(mq2ADC) + ",";
    response += "\"flame\":" + String(flameState) + ",";
    response += "\"temperature\":" + String(temp) + ",";
    response += "\"humidity\":" + String(hum);
    response += "}";

    server.send(200, "application/json", response);
    Serial.println("/alarm POST response sent");
  });

  server.on("/test-alarm", HTTP_POST, []() {
    Serial.println("Manual test alarm triggered from web!");
    triggerAlarmOutputs(true);
    triggerMobileAlert(999, 1, 25.0, 50.0);
    server.send(200, "application/json",
                "{\"status\":\"test_alarm_triggered\", \"message\":\"Cloud test alert sent\"}");
  });

  server.on("/status", HTTP_GET, []() {
    String status = "{";
    status += "\"mq2\": " + String(analogRead(MQ2_PIN)) + ",";
    status += "\"flame\": " + String(digitalRead(FLAME_PIN)) + ",";
    status += "\"alarm\": " + String(lastAlarmState ? "true" : "false") + ",";
    status += "\"wifi\": \"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\"";
    status += "}";
    server.send(200, "application/json", status);
  });

  server.begin();
  Serial.println("HTTP Server started on port 80");
}

// ================== ALARM OUTPUT CONTROL ==================
void triggerAlarmOutputs(bool activate) {
  if (activate) {
    digitalWrite(LED_PIN, HIGH);
    ledcWrite(PWM_CHANNEL, 128);
    Serial.println("Alarm outputs ACTIVATED");
  } else {
    digitalWrite(LED_PIN, LOW);
    ledcWrite(PWM_CHANNEL, 0);
    Serial.println("Alarm outputs DEACTIVATED");
  }
}

// ================== MACRODROID CLOUD ALERT FUNCTION ==================
void triggerMobileAlert(int mq2Value, int flameState, float temp, float hum) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = macroDroidCloudURL;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    DynamicJsonDocument doc(512);
    doc["gas_level"] = mq2Value;
    doc["flame_detected"] = (flameState == FLAME_TRIGGER);
    doc["temperature"] = temp;
    doc["humidity"] = hum;
    doc["alarm_time"] = millis();
    doc["message"] = "FIRE_ALARM_TRIGGERED";
    doc["severity"] = "HIGH";
    doc["device_ip"] = WiFi.localIP().toString();

    String payload;
    serializeJson(doc, payload);

    Serial.println("Sending MacroDroid Cloud Alert...");
    int httpResponseCode = http.POST(payload);
    Serial.print("Cloud response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 200) {
      Serial.println("Cloud alert sent successfully!");
    } else if (httpResponseCode == 404) {
      Serial.println("404 - Check MacroDroid trigger ID");
    } else if (httpResponseCode == -1) {
      Serial.println("Connection failed - timeout");
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected for cloud alert");
  }
}

// ================== TELEGRAM FUNCTIONS ==================
void sendTelegramMessage(const char* message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += telegramToken;
    url += "/sendMessage";

    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "chat_id=";
    payload += chatId;
    payload += "&text=";
    payload += message;

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      Serial.println("Telegram sent");
    } else {
      Serial.print("Telegram failed: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void sendTelegramMessage(String message) {
  sendTelegramMessage(message.c_str());
}

// ================== ALERT FUNCTIONS ==================
void sendFireAlert(int mq2Value, int flameState, float temp, float hum) {
  unsigned long currentMillis = millis();

  String alertMessage = "FIRE ALERT!\n\n";
  alertMessage += "Emergency: Fire detected!\n\n";
  alertMessage += "Sensor Readings:\n";
  alertMessage += "- Gas Level: " + String(mq2Value) + "\n";
  alertMessage += "- Flame: " + String(flameState == FLAME_TRIGGER ? "DETECTED" : "Safe") + "\n";
  alertMessage += "- Temperature: " + String(temp) + "C\n";
  alertMessage += "- Humidity: " + String(hum) + "%\n\n";
  alertMessage += "Triggering cloud alarm...";

  sendTelegramMessage(alertMessage);

  if (currentMillis - lastMobileAlert > mobileCooldown) {
    triggerMobileAlert(mq2Value, flameState, temp, hum);
    lastMobileAlert = currentMillis;
  }
}

void sendAllClearMessage() {
  String message = "ALL CLEAR\n\n";
  message += "Fire alarm condition has been resolved.\n";
  message += "All sensors are back to normal.\n";
  message += "System continues monitoring...";
  sendTelegramMessage(message);
}

// ================== MQTT FUNCTIONS ==================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Empty callback - required by library
}

void reconnectMQTT() {
  int retryCount = 0;
  while (!client.connected() && retryCount < 5) {
    retryCount++;
    Serial.print("Attempting MQTT connection (attempt ");
    Serial.print(retryCount);
    Serial.println(")...");

    if (client.connect("ESP32_FireAlarm")) {
      Serial.println("MQTT connected!");
      return;
    } else {
      delay(2000);
    }
  }
}

void updateWiFiStatus() {
  static unsigned long lastWiFiCheck = 0;
  static bool lastWiFiStatus = false;

  if (millis() - lastWiFiCheck > 5000) {
    lastWiFiCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (lastWiFiStatus) Serial.println("WiFi disconnected!");
      lastWiFiStatus = false;
    } else {
      if (!lastWiFiStatus) Serial.println("WiFi connected!");
      lastWiFiStatus = true;
    }
  }
}

// ================== MAIN LOOP ==================
void loop() {
  unsigned long currentMillis = millis();

  server.handleClient();
  updateWiFiStatus();

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int mq2ADC = analogRead(MQ2_PIN);
    int flameState = digitalRead(FLAME_PIN);
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (isnan(temp)) temp = 0;
    if (isnan(hum)) hum = 0;

    Serial.print("MQ2:"); Serial.print(mq2ADC);
    Serial.print(" Flame:"); Serial.print(flameState);
    Serial.print(" Temp:"); Serial.print(temp);
    Serial.print(" Hum:"); Serial.print(hum);

    bool mq2Alarm = (mq2ADC > MQ2_THRESHOLD);
    bool flameAlarm = (flameState == FLAME_TRIGGER);
    bool alarm = mq2Alarm || flameAlarm;

    Serial.print(" MQ2_Alarm:"); Serial.print(mq2Alarm);
    Serial.print(" Flame_Alarm:"); Serial.print(flameAlarm);
    Serial.print(" Overall_Alarm:"); Serial.println(alarm);

    if (alarm && !lastAlarmState) {
      Serial.println("ALARM TRIGGERED - Sending alerts!");
      sendFireAlert(mq2ADC, flameState, temp, hum);
      lastTelegramAlert = currentMillis;
    } else if (!alarm && lastAlarmState) {
      Serial.println("ALARM CLEARED");
      sendAllClearMessage();
    }
    lastAlarmState = alarm;

    triggerAlarmOutputs(alarm);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("FIRE ALARM SYSTEM");

    display.setCursor(0, 8);
    display.print(WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    display.println(client.connected() ? " MQTT: OK" : " MQTT: No");

    display.setCursor(0, 16);
    display.print("MQ2: "); display.println(mq2ADC);
    display.setCursor(0, 24);
    display.print("Flame: "); display.println(flameState == FLAME_TRIGGER ? "DETECTED" : "Safe");
    display.setCursor(0, 32);
    display.print("Temp: "); display.print(temp); display.println(" C");
    display.setCursor(0, 40);
    display.print("Hum: "); display.print(hum); display.println(" %");

    if (alarm) {
      display.setCursor(0, 48);
      display.println("!!! FIRE ALERT !!!");
      display.setCursor(0, 56);
      display.println("Cloud Alert Sent");
    } else {
      display.setCursor(0, 48);
      display.println("System: Normal");
      display.setCursor(0, 56);
      display.print("IP: ");
      display.println(WiFi.localIP());
    }
    display.display();

    if (client.connected()) {
      client.publish("fire/mq2", String(mq2ADC).c_str());
      client.publish("fire/flame", flameState == FLAME_TRIGGER ? "1" : "0");
      client.publish("fire/alarm", alarm ? "1" : "0");
      client.publish("fire/temp", String(temp).c_str());
      client.publish("fire/hum", String(hum).c_str());
    }
  }

  delay(10);
}
