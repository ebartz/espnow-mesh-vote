#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

#define HEARTBEAT_INTERVAL_MS 5000

enum MessageType : uint8_t {
  DATA = 0,
  READY = 1,
  KEEP_ALIVE = 2,
  ACK = 3
};

struct MeshMessage {
  uint8_t sender_mac[6];
  uint32_t msg_id;
  uint32_t timestamp;
  uint8_t ttl;
  MessageType type;
};

uint32_t lastHeartbeat = 0;

void printMac(const uint8_t* mac) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void sendAck(const uint8_t* dest_mac, uint32_t id) {
  // Add peer if not already known
  if (!esp_now_is_peer_exist(dest_mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, dest_mac, 6);
    peer.channel = 1;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.print("❌ Failed to add peer for ACK: ");
      printMac(dest_mac);
      Serial.println();
      return;
    }
  }

  MeshMessage ack;
  WiFi.macAddress(ack.sender_mac);
  ack.msg_id = id;
  ack.timestamp = millis();
  ack.ttl = 1;
  ack.type = ACK;

  esp_err_t result = esp_now_send(dest_mac, (uint8_t *)&ack, sizeof(ack));
  if (result == ESP_OK) {
    Serial.print("✅ Sent ACK to ");
    printMac(dest_mac);
    Serial.print(" | ID: ");
    Serial.println(id);
  } else {
    Serial.print("❌ Failed to send ACK to ");
    printMac(dest_mac);
    Serial.println();
  }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  Serial.print("📥 Packet received from: ");
  printMac(info->src_addr);
  Serial.print(" | Size: ");
  Serial.println(len);

  if (len != sizeof(MeshMessage)) {
    Serial.println("⚠️ Invalid message length");
    return;
  }

  MeshMessage msg;
  memcpy(&msg, incomingData, sizeof(MeshMessage));

  Serial.print("[");
  printMac(msg.sender_mac);
  Serial.print("] ");

  switch (msg.type) {
    case DATA:
      Serial.printf("🔘 DATA | ID: %lu | TTL: %d | Timestamp: %lu\n", msg.msg_id, msg.ttl, msg.timestamp);
      sendAck(info->src_addr, msg.msg_id);
      break;
    case READY:
      Serial.printf("✅ READY | ID: %lu | Timestamp: %lu\n", msg.msg_id, msg.timestamp);
      sendAck(info->src_addr, msg.msg_id);
      break;
    case KEEP_ALIVE:
      Serial.printf("📤 KEEP_ALIVE | ID: %lu | Timestamp: %lu\n", msg.msg_id, msg.timestamp);
      sendAck(info->src_addr, msg.msg_id);
      break;
    case ACK:
      Serial.println("📨 Received ACK (ignored)");
      break;
    default:
      Serial.println("❓ Unknown message type");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("🔧 Booting...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_wifi_set_max_tx_power(50);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  esp_wifi_start();

  Serial.print("📡 Receiver MAC: ");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  printMac(mac);
  Serial.println();

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t broadcastPeer = {};
  memset(broadcastPeer.peer_addr, 0xFF, 6);
  broadcastPeer.channel = 1;
  broadcastPeer.encrypt = false;

  if (esp_now_add_peer(&broadcastPeer) == ESP_OK) {
    Serial.println("✅ Broadcast peer added");
  } else {
    Serial.println("⚠️ Failed to add broadcast peer");
  }

  Serial.println("📡 Receiver ready");
  lastHeartbeat = millis();
}

void loop() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
    Serial.println("💓 Receiver alive...");
    lastHeartbeat = millis();
  }
  delay(10);
}
