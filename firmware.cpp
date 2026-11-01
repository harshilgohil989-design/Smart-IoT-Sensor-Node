// ESP32-C3 IoT Node - Wokwi simulation
// DHT22 + SSD1306 OLED (I2C) + button (interrupt, debounced) + relay + Wi-Fi + MQTT
// FreeRTOS tasks: sensor, display, network. loop() handles the button/relay.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ---------------- Pins (ESP32-C3, same as PCB plan) ----------------
#define PIN_DHT    3
#define PIN_BTN    4     // simulates 24V input (on PCB: optocoupler output, active-low)
#define PIN_RELAY  10
#define PIN_SDA    6
#define PIN_SCL    7

#define RELAY_ON_LEVEL HIGH   // change to LOW if your relay is active-low

// ---------------- Config ----------------
#define DEVICE_ID      "node01"
// !!! Change "yourname-4821" to something unique, the HiveMQ broker is public !!!
#define TOPIC_PREFIX   "iot/yourname-4821/" DEVICE_ID
#define TOPIC_TELEM    TOPIC_PREFIX "/telemetry"
#define TOPIC_CMD      TOPIC_PREFIX "/cmd"

#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_PASS      ""
#define WIFI_CHANNEL   6

#define MQTT_HOST      "broker.hivemq.com"
#define MQTT_PORT      1883

#define SENSOR_PERIOD_MS   3000   // 2-5 s required
#define PUBLISH_PERIOD_MS  5000
#define DEBOUNCE_MS        50

// ---------------- Objects ----------------
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
DHT dht(PIN_DHT, DHT22);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ---------------- Shared state ----------------
volatile float    g_temp = 0, g_hum = 0;
volatile bool     g_sensorOk = false;
volatile bool     g_inputOn = false;
volatile bool     g_relayOn = false;
volatile bool     g_wifiOk = false;
volatile bool     g_mqttOk = false;
volatile bool     g_publishNow = false;
bool              g_oledOk = false;

// ISR -> loop communication
volatile uint32_t g_lastEdge = 0;
volatile bool     g_edgeFlag = false;

// ---------------- Relay ----------------
void setRelay(bool on) {
  g_relayOn = on;
  digitalWrite(PIN_RELAY, on ? RELAY_ON_LEVEL : !RELAY_ON_LEVEL);
  g_publishNow = true;   // report change immediately
}

// ---------------- Button interrupt ----------------
// ISR only records that an edge happened; debounce is done in loop().
void IRAM_ATTR onButtonEdge() {
  g_lastEdge = millis();
  g_edgeFlag = true;
}

// ---------------- MQTT callback ----------------
// Accepts: ON / OFF / TOGGLE   or JSON like {"relay":1} / {"relay":0}
void mqttCallback(char *topic, byte *payload, unsigned int len) {
  char msg[64];
  unsigned int n = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = '\0';
  for (unsigned int i = 0; i < n; i++) msg[i] = toupper(msg[i]);

  if (strstr(msg, "TOGGLE"))                                 setRelay(!g_relayOn);
  else if (strstr(msg, "OFF") || strstr(msg, "\"RELAY\":0")) setRelay(false);
  else if (strstr(msg, "ON")  || strstr(msg, "\"RELAY\":1")) setRelay(true);
  Serial.printf("MQTT cmd: %s\n", msg);
}

// ---------------- Publish ----------------
void publishTelemetry() {
  char buf[160];
  if (g_sensorOk) {
    snprintf(buf, sizeof(buf),
             "{\"device\":\"%s\",\"temp\":%.1f,\"hum\":%.0f,\"input\":%d,\"relay\":%d}",
             DEVICE_ID, (float)g_temp, (float)g_hum, g_inputOn ? 1 : 0, g_relayOn ? 1 : 0);
  } else {
    snprintf(buf, sizeof(buf),
             "{\"device\":\"%s\",\"error\":\"sensor\",\"input\":%d,\"relay\":%d}",
             DEVICE_ID, g_inputOn ? 1 : 0, g_relayOn ? 1 : 0);
  }
  mqtt.publish(TOPIC_TELEM, buf);
  Serial.println(buf);
}

// ---------------- Task: sensor ----------------
void sensorTask(void *) {
  dht.begin();
  vTaskDelay(pdMS_TO_TICKS(2000));  // DHT22 needs ~2 s after power up
  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      g_sensorOk = false;
      Serial.println("Sensor read failed");
    } else {
      g_temp = t;
      g_hum = h;
      g_sensorOk = true;
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}

// ---------------- Task: display ----------------
void displayTask(void *) {
  for (;;) {
    if (g_oledOk) {
      char line[24];
      oled.clearDisplay();
      oled.setTextColor(SSD1306_WHITE);

      // Row 1: temperature + humidity (big) or error
      oled.setTextSize(2);
      oled.setCursor(0, 0);
      if (g_sensorOk) {
        snprintf(line, sizeof(line), "%.1fC %.0f%%", (float)g_temp, (float)g_hum);
        oled.print(line);
      } else {
        oled.print("SENSOR ERR");
      }

      // Rows 2-4: small text
      oled.setTextSize(1);
      oled.setCursor(0, 22);
      oled.printf("IN:%s  RELAY:%s", g_inputOn ? "ON" : "OFF", g_relayOn ? "ON" : "OFF");

      oled.setCursor(0, 34);
      oled.printf("WiFi:%s MQTT:%s", g_wifiOk ? "OK" : "--", g_mqttOk ? "OK" : "--");

      uint32_t s = millis() / 1000;
      oled.setCursor(0, 46);
      oled.printf("Uptime %02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

      oled.display();
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// ---------------- Task: network ----------------
void networkTask(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(256);
  mqtt.setSocketTimeout(3);

  uint32_t lastWifiTry = 0, lastMqttTry = 0, lastPub = 0;

  for (;;) {
    uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      g_wifiOk = false;
      g_mqttOk = false;
      if (now - lastWifiTry > 10000) {          // retry every 10 s, non-blocking
        lastWifiTry = now;
        Serial.println("WiFi reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
      }
    } else {
      g_wifiOk = true;
      if (!mqtt.connected()) {
        g_mqttOk = false;
        if (now - lastMqttTry > 5000) {         // retry every 5 s
          lastMqttTry = now;
          String cid = String(DEVICE_ID) + "-" + String((uint32_t)esp_random(), HEX);
          if (mqtt.connect(cid.c_str())) {
            mqtt.subscribe(TOPIC_CMD);
            g_mqttOk = true;
            g_publishNow = true;
            Serial.println("MQTT connected");
          }
        }
      } else {
        g_mqttOk = true;
        mqtt.loop();
        if (g_publishNow || now - lastPub >= PUBLISH_PERIOD_MS) {
          g_publishNow = false;
          lastPub = now;
          publishTelemetry();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, !RELAY_ON_LEVEL);
  pinMode(PIN_BTN, INPUT_PULLUP);
  g_inputOn = (digitalRead(PIN_BTN) == LOW);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN), onButtonEdge, CHANGE);

  Wire.begin(PIN_SDA, PIN_SCL);
  g_oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!g_oledOk) Serial.println("OLED init failed (continuing without display)");

  xTaskCreate(sensorTask,  "sensor",  4096, NULL, 1, NULL);
  xTaskCreate(displayTask, "display", 4096, NULL, 1, NULL);
  xTaskCreate(networkTask, "network", 8192, NULL, 2, NULL);
}

// loop(): debounce button, toggle relay. No blocking delay().
void loop() {
  if (g_edgeFlag && (millis() - g_lastEdge) >= DEBOUNCE_MS) {
    g_edgeFlag = false;
    bool pressed = (digitalRead(PIN_BTN) == LOW);
    if (pressed != g_inputOn) {
      g_inputOn = pressed;
      if (pressed) setRelay(!g_relayOn);   // toggle on press
      else g_publishNow = true;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(5));
}