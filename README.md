# espnow-mesh-vote

**espnow-mesh-vote** is a mesh-based wireless input system built on ESP32-C3 microcontrollers using ESP-NOW. It's designed for distributed environments where devices (nodes) send signals—such as button presses—to a central receiver without relying on traditional Wi-Fi infrastructure.

> ⚠️ This project is still in early development. It's functional for testing, but not feature-complete or production-ready yet.

---

## What it's for

This project aims to make it easy to set up a lightweight mesh network of ESP32-C3 boards that can:

- Send simple input signals (e.g. button presses)
- Relay those signals through other nodes if they're out of range
- Confirm message delivery using acknowledgments (ACKs)
- Periodically send keep-alive messages to indicate online status
- Identify each node automatically via its MAC address

It’s well-suited for scenarios like classroom voting systems, workshop feedback buttons, or local interaction systems in areas without stable Wi-Fi.

---

## How it works

- Nodes communicate over ESP-NOW using broadcast and peer-to-peer messages.
- Each node has a unique hardware MAC and sends a message when a button is pressed.
- If the receiver is out of range, nearby nodes can forward messages (mesh behavior).
- The receiver responds with an ACK to confirm successful delivery.
- Nodes retry if no ACK is received within a short window.
- All nodes also send periodic keep-alive messages.

---

## Hardware requirements

- One ESP32-C3 board as a **receiver**
- One or more ESP32-C3 boards as **senders**
- Momentary push button per sender (or use the built-in BOOT button)
- Optional: pull-down resistors (depending on button wiring)

---

## Software requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- ESP32 platform support (via Board Manager)
- This repository (`sender.ino` and `receiver.ino`)

---

## Getting started

1. Flash `receiver.ino` to the ESP32-C3 that will collect messages
2. Flash `sender.ino` to one or more ESP32-C3 boards
3. Open the Serial Monitor on the receiver to observe messages
4. Press a button on any sender to generate a message

All devices should be on the same Wi-Fi channel (channel 1 is used by default).

---

## Current features

- ✅ One-to-many messaging via ESP-NOW broadcast
- ✅ Multi-hop message relaying (mesh)
- ✅ ACK + retransmission logic for reliable delivery
- ✅ Unique sender identification (MAC address)
- ✅ Periodic keep-alive signals
- ✅ Basic serial output for monitoring

---

## Limitations / To-dos

- No persistent routing logic (only TTL-based relaying)
- No encryption or authentication (yet)
- No OTA updates or GUI interface
- No battery/power management features

---

## Roadmap ideas

- [ ] Web dashboard or OLED display support
- [ ] MQTT or USB serial output for integration
- [ ] Per-node status tracking on the receiver
- [ ] Adjustable power settings for better battery life
- [ ] Smarter relaying logic (RSSI-based, adaptive TTL)

---

## Contributing

PRs and issues are welcome! Just be aware this is still an early-stage side project, so things may change or break along the way.
