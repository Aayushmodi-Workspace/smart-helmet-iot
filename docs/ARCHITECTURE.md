# Updated Architecture - Multiple Helmets + Dual Master Nodes

This update changes the project from a single transmitter → single receiver
setup into a **multi-helmet, dual-master** setup, matching a real mine layout.

```
Minor Helmet #1 ─┐
Minor Helmet #2 ─┼──ESP-NOW──▶  MASTER 1 (inside mine)  ──ESP-NOW──▶  MASTER 2 (outside mine)  ──MQTT──▶  Cloud / Dashboard
Minor Helmet #3 ─┘
```

## Why two masters?

ESP-NOW has a limited range and mines are long/underground, so WiFi/internet
usually isn't available deep inside. So:

- **Master 1** sits right at the start of the mine / as deep as ESP-NOW can
  reach the workers. It just collects data from every helmet and relays it
  further out — it never touches WiFi or MQTT.
- **Master 2** sits outside the mine where normal WiFi/internet works. It
  receives the relayed data from Master 1 and is the only node that talks
  to the MQTT broker / cloud.

This "relay chain" can be extended later (Master 1 → Master 1B → Master 2)
if the mine is very long, without changing the helmet code at all.

## New files

| File | Runs on | Purpose |
|---|---|---|
| `minor-helmet/minor_helmet.ino` | Every worker's helmet | Reads sensors, sends data to Master 1 via ESP-NOW |
| `master1-inmine/master1_inmine.ino` | Coordinator inside the mine | Receives from all helmets, forwards to Master 2 via ESP-NOW |
| `master2-gateway/master2_gateway.ino` | Coordinator outside the mine | Receives from Master 1, connects to WiFi + MQTT, publishes to cloud |
| `docs/get_mac_address.ino` | Any ESP32 (temporary) | Prints the board's MAC address so you can fill it into the sketches above |

## Setup steps

1. **Find MAC addresses first.** Flash `docs/get_mac_address.ino` to your
   Master 1 board and your Master 2 board (one at a time), and note down
   each MAC address from the Serial Monitor.

2. **Minor helmets:**
   - Open `minor-helmet/minor_helmet.ino`.
   - Set a unique `HELMET_ID` for each helmet (1, 2, 3, ...).
   - Paste Master 1's MAC address into `master1Address`.
   - Flash it to each helmet's ESP32.

3. **Master 1 (in-mine):**
   - Open `master1-inmine/master1_inmine.ino`.
   - Paste Master 2's MAC address into `master2Address`.
   - Flash it to the in-mine coordinator ESP32.
   - This board does **not** need WiFi/internet, only power.

4. **Master 2 (outside gateway):**
   - Open `master2-gateway/master2_gateway.ino`.
   - Fill in your WiFi SSID/password and MQTT broker details.
   - Flash it to the outside coordinator ESP32.

5. Power everything on. Watch Serial Monitor on Master 2 — you should see
   data arriving from each helmet, tagged with its `helmetID`, and getting
   published to MQTT topics like:
   ```
   emqx/esp32/iot_project/helmet1
   emqx/esp32/iot_project/helmet2
   ```

## Data packet shared by all three sketches

All three `.ino` files use the **exact same struct** — this is required for
ESP-NOW to correctly interpret the bytes it receives:

```cpp
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
```

If you ever add a new sensor or field, update it in **all three files**
identically, otherwise the received data will be garbled.

## Notes / things you can improve later

- Right now, Master 1 forwards data the instant it receives it (no
  batching). That's simplest to understand and debug, and is fine for a
  handful of helmets.
- You can add a small OLED screen to Master 1 to show live helmet status
  even without internet.
- If your mine is very long, you can chain multiple "Master 1"-style
  relay nodes before reaching Master 2.
