/*
  Tiny helper sketch - flash this to ANY ESP32 first to find its
  MAC address. You need this MAC address to fill into the other
  sketches (master1Address, master2Address, etc.)

  Steps:
  1. Upload this sketch to the board.
  2. Open Serial Monitor at 115200 baud.
  3. Copy the printed MAC address.
  4. Convert it into the {0x.., 0x.., ...} format used in the
     other sketches (see example below).
*/

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.print("This board's MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Copy this into the receiving board's code, e.g.:");
  Serial.println("uint8_t someAddress[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};");
}

void loop() {
  // nothing to do
}
