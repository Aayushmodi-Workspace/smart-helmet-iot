/*
  ============================================================
  SMART HELMET - MASTER 2 (OUTSIDE-MINE CLOUD GATEWAY)
  ============================================================
  This code runs on the ESP32 placed OUTSIDE the mine, where
  normal WiFi/internet is available.

  What it does:
  1. Listens for ESP-NOW packets forwarded by MASTER 1
     (which is relaying data from all the minor helmets).
  2. Connects to WiFi and an MQTT broker.
  3. Converts each received packet into a JSON message and
     publishes it to the cloud (MQTT), tagged with the
     helmet ID so you know which worker it belongs to.
  4. Also sounds a local alert (buzzer/LED) if any helmet's
     latest reading crosses a dangerous threshold - useful
     for a control-room style display outside the mine.
  ============================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_now.h>
#include <ArduinoJson.h>

// ---------- WiFi credentials ----------
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ---------- MQTT broker settings ----------
const char* mqtt_broker   = "broker.emqx.io";
const char* mqtt_username = "emqx";
const char* mqtt_password = "public";
const int   mqtt_port     = 1883;

// All helmet data is published under this base topic, with the
// helmet ID appended, e.g. "emqx/esp32/iot_project/helmet1"
const char* topic_base = "emqx/esp32/iot_project/helmet";

WiFiClient   espClient;
PubSubClient client(espClient);

// ---------- DATA PACKET (must match minor_helmet.ino / master1_inmine.ino) ----------
typedef struct struct_message {
  int   helmetID;
  float temperature;
  int   gasValue;
  int   irValue;
  int16_t ax, ay, az;

  bool tempConnected;
  bool gasConnected;
  bool irConnected;
  bool mpuConnected;
} struct_message;

struct_message incomingData;

// Alert thresholds - keep these the same as on the minor helmets
const int   GAS_THRESHOLD  = 1500;
const float FALL_THRESHOLD = 15.0;
const float TEMP_THRESHOLD = 40.0;

#define BUZZER_PIN 4
#define LED_PIN    2

// ---------- WiFi + MQTT connection helpers ----------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    String clientId = "esp32-master2-" + String(WiFi.macAddress());
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println(" connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 2 seconds");
      delay(2000);
    }
  }
}

// Checks the latest packet for danger and buzzes if needed
void checkAlerts(const struct_message &data) {
  bool alert = false;

  if (data.gasConnected && data.gasValue > GAS_THRESHOLD) alert = true;
  if (data.tempConnected && data.temperature > TEMP_THRESHOLD) alert = true;
  if (data.irConnected && data.irValue == LOW) alert = true;

  if (data.mpuConnected) {
    float axF = data.ax / 1000.0;
    float ayF = data.ay / 1000.0;
    float azF = data.az / 1000.0;
    if (abs(axF) > FALL_THRESHOLD || abs(ayF) > FALL_THRESHOLD || abs(azF) > FALL_THRESHOLD) {
      alert = true;
    }
  }

  digitalWrite(BUZZER_PIN, alert ? HIGH : LOW);
  digitalWrite(LED_PIN, alert ? HIGH : LOW);
}

// Builds the JSON payload and publishes it to MQTT
void publishToMQTT(const struct_message &data) {
  StaticJsonDocument<256> doc;

  doc["helmetID"] = data.helmetID;
  if (data.tempConnected) doc["temperature"] = data.temperature;
  if (data.gasConnected)  doc["gasValue"]    = data.gasValue;
  if (data.irConnected)   doc["irValue"]     = data.irValue;
  if (data.mpuConnected) {
    doc["accel_x"] = data.ax;
    doc["accel_y"] = data.ay;
    doc["accel_z"] = data.az;
  }

  char payload[256];
  serializeJson(doc, payload);

  // Each helmet gets its own topic: .../helmet1, .../helmet2, etc.
  String topic = String(topic_base) + String(data.helmetID);

  Serial.print("Publishing to ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(payload);

  client.publish(topic.c_str(), payload);
}

// Called automatically whenever Master 1 sends us data
void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingData)) {
    Serial.println("Received packet with wrong size - ignoring");
    return;
  }

  memcpy(&incomingData, data, len);

  Serial.println("----------------------------------------");
  Serial.print("Data received (via Master 1) from Helmet #");
  Serial.println(incomingData.helmetID);

  checkAlerts(incomingData);

  if (!client.connected()) {
    connectMQTT();
  }
  publishToMQTT(incomingData);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Connect to WiFi and MQTT first
  connectWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  connectMQTT();

  // Then start ESP-NOW to receive from Master 1.
  // Note: ESP-NOW works fine alongside an active WiFi STA connection.
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(onDataReceived);

  Serial.println("Master 2 (cloud gateway) ready. Waiting for data from Master 1...");
}

void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
}
