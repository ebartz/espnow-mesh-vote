#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

#define BUTTON_PIN 9
#define TTL_INITIAL 5
#define KEEP_ALIVE_INTERVAL_MS 10000
#define HEARTBEAT_INTERVAL_MS 5000
#define RETRY_TIMEOUT_MS 3000
#define MAX_RETRIES 5
#define MSG_CACHE_SIZE 20

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

uint8_t self_mac[6];
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t messageCounter = 0;
uint32_t lastButtonState = HIGH;
uint32_t lastKeepAlive = 0;
uint32_t lastHeartbeat = 0;

struct PendingMessage {
  MeshMessage msg;
  uint32_t lastSendTime;
  uint8_t retryCount;
  bool awaitingAck;
  bool hasMessage;
};

PendingMessage pending;
bool queuedDataMessage = false;

struct RecentMsg {
  uint8_t sender_mac[6];
  uint32_t msg_id;
};

RecentMsg recentMessages[MSG_CACHE_SIZE];
uint8_t recentIndex = 0;

bool isDuplicate(uint8_t* sender, uint32_t id) {
  for (int i = 0; i < MSG_CACHE_SIZE; i++) {
    if (memcmp(sender, recentMessages[i].sender_mac, 6) == 0 && recentMessages[i].msg_id == id) {
      return true;
    }
  }
  return false;
}

void storeMessageId(uint8_t* sender, uint32_t id) {
  memcpy(recentMessages[recentIndex].sender_mac, sender, 6);
  recentMessages[recentIndex].msg_id = id;
  recentIndex = (recentIndex + 1) % MSG_CACHE_SIZE;
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(MeshMessage)) return;

  MeshMessage msg;
  memcpy(&msg, incomingData, sizeof(MeshMessage));

  if (isDuplicate(msg.sender_mac, msg.msg_id)) return;
  storeMessageId(msg.sender_mac, msg.msg_id);

  if (msg.type == ACK && msg.msg_id == pending.msg.msg_id) {
    pending.awaitingAck = false;
    pending.hasMessage = false;
    Serial.printf("✅ ACK received for ID %lu\n", msg.msg_id);
    return;
  }

  // Print relayed message
  Serial.printf("🔁 Relaying message from ");
  for (int i = 0; i < 6; i++) Serial.printf("%02X:", msg.sender_mac[i]);
  Serial.printf(" ID: %lu, TTL: %u\n", msg.msg_id, msg.ttl);

  // Relay if TTL > 1 and not from self
  if (msg.ttl > 1 && memcmp(msg.sender_mac, self_mac, 6) != 0) {
    msg.ttl--;
    esp_now_send(broadcast_mac, (uint8_t*)&msg, sizeof(msg));
  }
}

void sendMessage(MeshMessage& msg) {
  msg.timestamp = millis();
  esp_now_send(broadcast_mac, (uint8_t *)&msg, sizeof(msg));
  Serial.printf("📤 Sent %s | ID: %lu | TTL: %d\n",
                msg.type == DATA ? "DATA" :
                msg.type == READY ? "READY" :
                msg.type == KEEP_ALIVE ? "KEEP_ALIVE" : "???",
                msg.msg_id, msg.ttl);
}

void queueMessage(MessageType type) {
  if (pending.awaitingAck) {
    if (type == DATA) queuedDataMessage = true;
    return;
  }

  messageCounter++;

  MeshMessage msg;
  memcpy(msg.sender_mac, self_mac, 6);
  msg.msg_id = messageCounter;
  msg.ttl = TTL_INITIAL;
  msg.type = type;
  msg.timestamp = millis();

  pending.msg = msg;
  pending.lastSendTime = millis();
  pending.retryCount = 0;
  pending.awaitingAck = true;
  pending.hasMessage = true;

  sendMessage(pending.msg);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_wifi_set_max_tx_power(50);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  esp_wifi_start();

  WiFi.macAddress(self_mac);
  Serial.printf("📲 MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                self_mac[0], self_mac[1], self_mac[2],
                self_mac[3], self_mac[4], self_mac[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcast_mac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  queueMessage(READY);
  lastKeepAlive = millis();
  lastHeartbeat = millis();
}

void loop() {
  uint32_t now = millis();
  int buttonState = digitalRead(BUTTON_PIN);

  // Retry pending message
  if (pending.awaitingAck && now - pending.lastSendTime >= RETRY_TIMEOUT_MS) {
    if (pending.retryCount < MAX_RETRIES) {
      pending.retryCount++;
      pending.lastSendTime = now;
      Serial.printf("🔁 Retrying message ID %lu (attempt %d)\n", pending.msg.msg_id, pending.retryCount);
      sendMessage(pending.msg);
    } else {
      Serial.printf("❌ No ACK for message ID %lu after %d retries\n", pending.msg.msg_id, MAX_RETRIES);
      pending.awaitingAck = false;
      pending.hasMessage = false;
    }
  }

  // Send queued message
  if (!pending.awaitingAck && queuedDataMessage) {
    queuedDataMessage = false;
    queueMessage(DATA);
  }

  // Button
  if (buttonState == LOW && lastButtonState == HIGH) {
    queueMessage(DATA);
  }
  lastButtonState = buttonState;

  // Keep Alive
  if (now - lastKeepAlive > KEEP_ALIVE_INTERVAL_MS) {
    queueMessage(KEEP_ALIVE);
    lastKeepAlive = now;
  }

  // Heartbeat
  if (now - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
    Serial.println("💓 Sender alive...");
    lastHeartbeat = now;
  }

  delay(10);
}
