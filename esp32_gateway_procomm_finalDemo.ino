#include <WiFi.h>
#include <PubSubClient.h>
#include <NimBLEDevice.h>
#include <string.h>
#include <string>

// =====================================================
// WIFI / MQTT
// =====================================================

const char* WIFI_SSID = "Karthik";
const char* WIFI_PASSWORD = "12345678";

const char* MQTT_BROKER = "10.109.15.22";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "mine/test";

// =====================================================
// PACKET DEFINITIONS
// =====================================================

#define PACKET_SIZE 7
#define KAVACHAM_PROTOCOL 0xCA

#define GAS_UNSAFE_BIT   0x01
#define TEMP_UNSAFE_BIT  0x02
#define FALL_UNSAFE_BIT  0x04
#define MANUAL_SOS_BIT   0x08

// =====================================================
// OBJECTS
// =====================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

NimBLEScan* bleScan = nullptr;

// =====================================================
// PENDING BLE PACKET
// =====================================================

uint8_t pendingPacket[PACKET_SIZE];

volatile bool packetPending = false;

int pendingRSSI = 0;

char pendingAddress[40] = {0};

// =====================================================
// DUPLICATE PACKET TRACKING
// =====================================================

uint8_t lastSource = 0;
uint8_t lastSequence = 255;

bool haveLastPacket = false;

// =====================================================
// MQTT RECONNECT TIMING
// =====================================================

unsigned long lastMQTTAttempt = 0;

#define MQTT_RECONNECT_INTERVAL 3000

// =====================================================
// BLE CALLBACK
// =====================================================

class ScanCallbacks : public NimBLEScanCallbacks {

  void onResult(const NimBLEAdvertisedDevice* device) override {

    // Check device name
    if (!device->haveName()) {
      return;
    }

    std::string name = device->getName();

    if (name != "KAVACHAM") {
      return;
    }

    // Check manufacturer data
    if (!device->haveManufacturerData()) {
      return;
    }

    std::string manufacturerData = device->getManufacturerData();

    if (manufacturerData.length() != PACKET_SIZE) {
      return;
    }

    const uint8_t* rx =
      (const uint8_t*)manufacturerData.data();

    // Check protocol
    if (rx[0] != KAVACHAM_PROTOCOL) {
      return;
    }

    uint8_t source = rx[1];
    uint8_t sequence = rx[3];

    // Ignore exact duplicate
    if (haveLastPacket &&
        source == lastSource &&
        sequence == lastSequence) {

      return;
    }

    // Do not overwrite pending packet
    if (packetPending) {
      return;
    }

    // Copy packet
    memcpy(pendingPacket, rx, PACKET_SIZE);

    pendingRSSI = device->getRSSI();

    std::string address =
      device->getAddress().toString();

    strncpy(
      pendingAddress,
      address.c_str(),
      sizeof(pendingAddress) - 1
    );

    pendingAddress[sizeof(pendingAddress) - 1] = '\0';

    packetPending = true;
  }
};

ScanCallbacks scanCallbacks;

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 15000
  ) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi CONNECTED");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.print("WiFi FAILED. Status = ");
    Serial.println(WiFi.status());
  }
}

// =====================================================
// MQTT CONNECTION
// =====================================================

void connectMQTT() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  if (
    millis() - lastMQTTAttempt <
    MQTT_RECONNECT_INTERVAL
  ) {
    return;
  }

  lastMQTTAttempt = millis();

  Serial.println();
  Serial.println("Connecting to MQTT...");

  String clientID = "KAVACHAM_GATEWAY_";

  clientID += String(
    (uint32_t)(ESP.getEfuseMac() >> 32),
    HEX
  );

  clientID += String(
    (uint32_t)ESP.getEfuseMac(),
    HEX
  );

  if (
    mqttClient.connect(clientID.c_str())
  ) {

    Serial.println("MQTT CONNECTED");

  } else {

    Serial.print(
      "MQTT CONNECTION FAILED. State = "
    );

    Serial.println(
      mqttClient.state()
    );
  }
}

// =====================================================
// BLE SCAN START
// =====================================================

void startBLEScan() {

  if (bleScan == nullptr) {
    return;
  }

  if (bleScan->isScanning()) {
    return;
  }

  bleScan->clearResults();

  bleScan->setActiveScan(true);

  bleScan->start(
    1000,
    false
  );

  Serial.println("BLE SCAN STARTED");
}

// =====================================================
// MESSAGE TYPE
// =====================================================

enum MessageType {

  TELEMETRY = 0,

  GAS_THRESHOLD_EXCEEDED,

  TEMPERATURE_EXCEEDED,

  CRITICAL_FALL,

  MANUAL_SOS
};

int getMessageType(uint8_t status) {

  if (status & MANUAL_SOS_BIT) {
    return MANUAL_SOS;
  }

  if (status & FALL_UNSAFE_BIT) {
    return CRITICAL_FALL;
  }

  if (status & GAS_UNSAFE_BIT) {
    return GAS_THRESHOLD_EXCEEDED;
  }

  if (status & TEMP_UNSAFE_BIT) {
    return TEMPERATURE_EXCEEDED;
  }

  return TELEMETRY;
}

// =====================================================
// MESSAGE TYPE STRING
// =====================================================

const char* getMessageTypeString(
  int type
) {

  switch (type) {

    case MANUAL_SOS:
      return "MANUAL_SOS";

    case CRITICAL_FALL:
      return "CRITICAL_FALL";

    case GAS_THRESHOLD_EXCEEDED:
      return "GAS_THRESHOLD_EXCEEDED";

    case TEMPERATURE_EXCEEDED:
      return "TEMPERATURE_EXCEEDED";

    default:
      return "TELEMETRY";
  }
}

// =====================================================
// PROCESS BLE PACKET
// =====================================================

void processBLEPacket() {

  if (!packetPending) {
    return;
  }

  // Local copy
  uint8_t packet[PACKET_SIZE];

  memcpy(
    packet,
    pendingPacket,
    PACKET_SIZE
  );

  int rssi = pendingRSSI;

  char address[40];

  strncpy(
    address,
    pendingAddress,
    sizeof(address) - 1
  );

  address[sizeof(address) - 1] = '\0';

  packetPending = false;

  // ===================================================
  // DECODE PACKET
  // ===================================================

  uint8_t protocol =
    packet[0];

  uint8_t sourceNode =
    packet[1];

  uint8_t status =
    packet[2];

  uint8_t sequence =
    packet[3];

  uint8_t gas =
    packet[4];

  uint8_t temperature =
    packet[5];

  uint8_t nodeType =
    packet[6];

  // ===================================================
  // UPDATE DUPLICATE TRACKING
  // ===================================================

  lastSource = sourceNode;

  lastSequence = sequence;

  haveLastPacket = true;

  // ===================================================
  // DECODE STATUS
  // ===================================================

  bool gasUnsafe =
    status & GAS_UNSAFE_BIT;

  bool temperatureUnsafe =
    status & TEMP_UNSAFE_BIT;

  bool fall =
    status & FALL_UNSAFE_BIT;

  bool manualSOS =
    status & MANUAL_SOS_BIT;

  int messageType =
    getMessageType(status);

  const char* message =
    getMessageTypeString(messageType);

  // ===================================================
  // SERIAL OUTPUT
  // ===================================================

  Serial.println();

  Serial.println(
    "BLE PACKET RECEIVED"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "Protocol       : 0x"
  );

  Serial.println(
    protocol,
    HEX
  );

  Serial.print(
    "Source Node    : "
  );

  Serial.println(
    sourceNode
  );

  Serial.print(
    "Sequence       : "
  );

  Serial.println(
    sequence
  );

  Serial.print(
    "Gas            : "
  );

  Serial.println(
    gas
  );

  Serial.print(
    "Temperature    : "
  );

  Serial.println(
    temperature
  );

  Serial.print(
    "Gas Unsafe     : "
  );

  Serial.println(
    gasUnsafe ? "YES" : "NO"
  );

  Serial.print(
    "Temp Unsafe    : "
  );

  Serial.println(
    temperatureUnsafe ? "YES" : "NO"
  );

  Serial.print(
    "Fall           : "
  );

  Serial.println(
    fall ? "YES" : "NO"
  );

  Serial.print(
    "Manual SOS     : "
  );

  Serial.println(
    manualSOS ? "YES" : "NO"
  );

  Serial.print(
    "Node Type      : "
  );

  Serial.println(
    nodeType
  );

  Serial.print(
    "RSSI           : "
  );

  Serial.println(
    rssi
  );

  Serial.print(
    "BLE Address    : "
  );

  Serial.println(
    address
  );

  Serial.print(
    "MESSAGE        : "
  );

  Serial.println(
    message
  );

  Serial.println(
    "========================================"
  );

  // ===================================================
  // MQTT CHECK
  // ===================================================

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "MQTT SKIPPED - WiFi disconnected"
    );

    return;
  }

  if (!mqttClient.connected()) {

    connectMQTT();

    if (!mqttClient.connected()) {

      Serial.println(
        "MQTT SKIPPED - MQTT not connected"
      );

      return;
    }
  }

  // ===================================================
  // JSON
  // ===================================================

  String json = "{";

  json += "\"node\":";
  json += String(sourceNode);

  json += ",\"worker_id\":\"WSN-";
  json += String(sourceNode);
  json += "\"";

  json += ",\"message\":\"";
  json += message;
  json += "\"";

  json += ",\"sequence\":";
  json += String(sequence);

  json += ",\"gas\":";
  json += String(gas);

  json += ",\"temperature\":";
  json += String(temperature);

  json += ",\"gas_unsafe\":";
  json += gasUnsafe ? "true" : "false";

  json += ",\"temperature_unsafe\":";
  json += temperatureUnsafe ? "true" : "false";

  json += ",\"fall\":";
  json += fall ? "true" : "false";

  json += ",\"manual_sos\":";
  json += manualSOS ? "true" : "false";

  json += ",\"node_type\":";
  json += String(nodeType);

  json += ",\"rssi\":";
  json += String(rssi);

  json += ",\"ble_address\":\"";
  json += address;
  json += "\"";

  json += ",\"gateway\":\"KAVACHAM_GATEWAY\"";

  json += "}";

  // ===================================================
  // MQTT PUBLISH
  // ===================================================

  Serial.print(
    "MQTT Payload Length = "
  );

  Serial.println(
    json.length()
  );

  if (
    mqttClient.publish(
      MQTT_TOPIC,
      json.c_str()
    )
  ) {

    Serial.println(
      ">>> MQTT PUBLISHED <<<"
    );

  } else {

    Serial.println(
      "!!! MQTT PUBLISH FAILED !!!"
    );

    Serial.print(
      "MQTT State = "
    );

    Serial.println(
      mqttClient.state()
    );
  }
}

// =====================================================
// BLE SETUP
// =====================================================

void setupBLE() {

  NimBLEDevice::init(
    "KAVACHAM"
  );

  bleScan =
    NimBLEDevice::getScan();

  bleScan->setScanCallbacks(
    &scanCallbacks
  );

  bleScan->setActiveScan(
    true
  );

  bleScan->setInterval(
    45
  );

  bleScan->setWindow(
    30
  );

  startBLEScan();
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "      KAVACHAM GATEWAY"
  );

  Serial.println(
    "========================================"
  );

  // WiFi
  connectWiFi();

  // MQTT
  mqttClient.setServer(
    MQTT_BROKER,
    MQTT_PORT
  );

  // MQTT buffer increased ONLY for larger JSON
  mqttClient.setBufferSize(512);

  connectMQTT();

  // BLE
  setupBLE();

  Serial.println();
  Serial.println(
    "KAVACHAM GATEWAY READY"
  );

  Serial.println(
    "========================================"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // WIFI
  // ===================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    connectWiFi();
  }

  // ===================================================
  // MQTT
  // ===================================================

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    if (!mqttClient.connected()) {

      connectMQTT();

    } else {

      mqttClient.loop();
    }
  }

  // ===================================================
  // PROCESS BLE
  // ===================================================

  processBLEPacket();

  // ===================================================
  // RESTART BLE SCAN
  // ===================================================

  if (
    !packetPending &&
    !bleScan->isScanning()
  ) {

    startBLEScan();
  }

  delay(5);
}
