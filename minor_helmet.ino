/*
  ============================================================
  SMART HELMET - MINOR NODE (TRANSMITTER)
  ============================================================
  This code runs on EVERY minor's (worker's) helmet.

  What it does:
  1. Reads Temperature, Gas, IR, and Accelerometer sensors.
  2. Packs the readings into one "packet" of data.
  3. Sends that packet to MASTER 1 (the in-mine coordinator)
     using ESP-NOW (no WiFi/internet needed for this part).

  IMPORTANT: Every helmet must have its own unique HELMET_ID
  below, so Master 1 / Master 2 / Cloud can tell the helmets
  apart. Example: Helmet 1 -> HELMET_ID = 1, Helmet 2 -> 2, etc.
  ============================================================
*/

#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------- CHANGE THIS FOR EVERY HELMET ----------
#define HELMET_ID 1   // <-- Helmet 1 = 1, Helmet 2 = 2, Helmet 3 = 3 ...
// ---------------------------------------------------

// ---------- PIN DEFINITIONS ----------
#define DHTPIN 15
#define DHTTYPE DHT22
#define MQ135_PIN 34
#define IR_PIN 27
#define BUZZER_PIN 4

DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPU6050 mpu;

// MAC address of MASTER 1 (the in-mine coordinator).
// Replace with Master 1's actual MAC address.
uint8_t master1Address[] = {0x08, 0xA6, 0x1D, 0x6C, 0x8C, 0x30};

// Alert thresholds - change these to whatever is safe for your mine
const int   GAS_THRESHOLD  = 1500;
const float FALL_THRESHOLD = 15.0;
const float TEMP_THRESHOLD = 40.0;

// ---------- DATA PACKET SENT OVER ESP-NOW ----------
// NOTE: This exact same structure must also exist in
// master1_inmine.ino and master2_gateway.ino
typedef struct struct_message {
  int   helmetID;             // which helmet this data came from
  float temperature;
  int   gasValue;
  int   irValue;
  int16_t ax, ay, az;         // accelerometer values

  // "Connected" flags let Master/Cloud know if a sensor is
  // physically attached, so missing sensors don't cause false alerts.
  bool tempConnected;
  bool gasConnected;
  bool irConnected;
  bool mpuConnected;
} struct_message;

struct_message outgoingData;

// Callback: tells us if our last ESP-NOW send worked or not
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ---- Sensor setup ----
  dht.begin();

  outgoingData.mpuConnected = mpu.begin();
  if (!outgoingData.mpuConnected) {
    Serial.println("MPU6050 not found - continuing without it");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  }

  // We are assuming DHT and IR are always connected.
  // If you want to auto-detect them too, you can add checks here.
  outgoingData.tempConnected = true;
  outgoingData.gasConnected  = true;
  outgoingData.irConnected   = true;

  // Fixed helmet ID for this board
  outgoingData.helmetID = HELMET_ID;

  // ---- ESP-NOW setup ----
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, master1Address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Master 1 as peer");
    return;
  }

  Serial.print("Minor Helmet #");
  Serial.print(HELMET_ID);
  Serial.println(" ready.");
}

void loop() {
  // ---- Read sensors ----
  float temp = dht.readTemperature();
  outgoingData.temperature = isnan(temp) ? 0 : temp;

  outgoingData.gasValue = analogRead(MQ135_PIN);
  outgoingData.irValue  = digitalRead(IR_PIN);

  if (outgoingData.mpuConnected) {
    sensors_event_t a, g, tempEvent;
    mpu.getEvent(&a, &g, &tempEvent);
    // Convert to int16 just to keep the packet small
    outgoingData.ax = (int16_t)(a.acceleration.x * 1000);
    outgoingData.ay = (int16_t)(a.acceleration.y * 1000);
    outgoingData.az = (int16_t)(a.acceleration.z * 1000);
  }

  // ---- Check for local danger, buzz immediately if found ----
  bool alert = false;

  if (outgoingData.gasConnected && outgoingData.gasValue > GAS_THRESHOLD) alert = true;
  if (outgoingData.tempConnected && outgoingData.temperature > TEMP_THRESHOLD) alert = true;
  if (outgoingData.irConnected && outgoingData.irValue == LOW) alert = true; // helmet not worn
  if (outgoingData.mpuConnected) {
    float axF = outgoingData.ax / 1000.0;
    float ayF = outgoingData.ay / 1000.0;
    float azF = outgoingData.az / 1000.0;
    if (abs(axF) > FALL_THRESHOLD || abs(ayF) > FALL_THRESHOLD || abs(azF) > FALL_THRESHOLD) {
      alert = true;
    }
  }

  digitalWrite(BUZZER_PIN, alert ? HIGH : LOW);

  // ---- Send the packet to Master 1 ----
  esp_err_t result = esp_now_send(master1Address, (uint8_t *) &outgoingData, sizeof(outgoingData));

  if (result == ESP_OK) {
    Serial.println("Sent data to Master 1 successfully");
  } else {
    Serial.println("Error sending data to Master 1");
  }

  delay(2000); // send every 2 seconds
}
