/*
  ============================================================
  SMART HELMET - MASTER 1 (IN-MINE COORDINATOR)
  ============================================================
  This code runs on the ESP32 that sits AT THE START / INSIDE
  the mine, where there is no WiFi/internet.

  What it does:
  1. Listens for ESP-NOW packets from ALL minor helmets.
     (No need to know each helmet's MAC in advance - any
      helmet that sends a packet addressed to this board's
      MAC will be received.)
  2. Prints the data to Serial so you can see it locally.
  3. Immediately forwards ("relays") the same packet over
     ESP-NOW to MASTER 2, which is outside the mine and has
     internet access.

  Master 1 does NOT connect to WiFi or MQTT - it is only a
  relay between the minors and Master 2.
  ============================================================
*/

#include <esp_now.h>
#include <WiFi.h>

// MAC address of MASTER 2 (the outside-mine gateway node).
// Replace with Master 2's actual MAC address.
uint8_t master2Address[] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};

// ---------- DATA PACKET (must match minor_helmet.ino exactly) ----------
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

struct_message incomingData;   // data coming in from a minor helmet
struct_message forwardData;    // exact copy we forward to Master 2

// Optional: LED to show Master 1 is alive / relaying data
#define STATUS_LED 2

// Called automatically whenever ESP-NOW sends something out
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Forward to Master 2: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Called automatically whenever a minor helmet sends data to us
void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingData)) {
    Serial.println("Received packet with wrong size - ignoring");
    return;
  }

  memcpy(&incomingData, data, len);

  Serial.println("----------------------------------------");
  Serial.print("Data received from Helmet #");
  Serial.println(incomingData.helmetID);

  if (incomingData.tempConnected) {
    Serial.print("Temperature: ");
    Serial.println(incomingData.temperature);
  }
  if (incomingData.gasConnected) {
    Serial.print("Gas Value: ");
    Serial.println(incomingData.gasValue);
  }
  if (incomingData.irConnected) {
    Serial.print("IR Value: ");
    Serial.println(incomingData.irValue);
  }
  if (incomingData.mpuConnected) {
    Serial.print("Accel (x,y,z): ");
    Serial.print(incomingData.ax); Serial.print(", ");
    Serial.print(incomingData.ay); Serial.print(", ");
    Serial.println(incomingData.az);
  }

  digitalWrite(STATUS_LED, HIGH);

  // Simply copy the packet as-is and forward it to Master 2.
  // We are not changing anything - Master 2 will do the
  // MQTT/cloud formatting.
  forwardData = incomingData;

  esp_err_t result = esp_now_send(master2Address, (uint8_t *) &forwardData, sizeof(forwardData));
  if (result != ESP_OK) {
    Serial.println("Error forwarding data to Master 2");
  }

  digitalWrite(STATUS_LED, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);

  // ESP-NOW only needs WiFi in station mode, no actual WiFi
  // connection is made (no internet inside the mine).
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);

  // Add Master 2 as a peer so we are allowed to send to it
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, master2Address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Master 2 as peer");
    return;
  }

  Serial.println("Master 1 (in-mine) ready. Waiting for helmet data...");
}

void loop() {
  // Nothing to do here - everything happens inside onDataReceived()
  delay(10);
}
