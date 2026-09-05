#include <WiFi.h>
#include <PubSubClient.h>
#include <NimBLEDevice.h>

// =====================================================
// KAVACHAM GATEWAY
// BLE -> MQTT
// =====================================================

const char* WIFI_SSID = "Megha";
const char* WIFI_PASSWORD = "megha1356";

const char* MQTT_BROKER = "10.136.236.22";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "mine/test";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

NimBLEScan *bleScan;

// =====================================================
// MESSAGE TYPES
// =====================================================

#define MSG_HEARTBEAT       1
#define MSG_TELEMETRY       2
#define MSG_GAS_THRESHOLD   3
#define MSG_TEMP_THRESHOLD  4
#define MSG_CRITICAL_FALL   5
#define MSG_MANUAL_SOS      6
#define MSG_NODE_LOST       7
#define MSG_FALL_CANCELLED  8

// =====================================================
// MESSAGE NAME
// =====================================================

const char* messageName(uint8_t type)
{
  switch (type)
  {
    case MSG_HEARTBEAT:
      return "HEARTBEAT";

    case MSG_TELEMETRY:
      return "TELEMETRY";

    case MSG_GAS_THRESHOLD:
      return "GAS_THRESHOLD_EXCEEDED";

    case MSG_TEMP_THRESHOLD:
      return "TEMPERATURE_EXCEEDED";

    case MSG_CRITICAL_FALL:
      return "CRITICAL_FALL";

    case MSG_MANUAL_SOS:
      return "MANUAL_SOS";

    case MSG_NODE_LOST:
      return "NODE_LOST";

    case MSG_FALL_CANCELLED:
      return "FALL_CANCELLED";

    default:
      return "UNKNOWN";
  }
}

// =====================================================
// CHECKSUM
// =====================================================

uint8_t checksum(uint8_t *packet)
{
  uint8_t c = 0;

  for (int i = 0; i < 10; i++)
    c ^= packet[i];

  return c;
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi()
{
  Serial.print("Connecting WiFi");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (
    WiFi.status() != WL_CONNECTED
  )
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");

  Serial.print("Gateway IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// MQTT
// =====================================================

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Connecting MQTT... ");

    String clientID =
      "KAVACHAM-GATEWAY-" +
      String(
        (uint32_t)ESP.getEfuseMac(),
        HEX
      );

    if (
      mqttClient.connect(
        clientID.c_str()
      )
    )
    {
      Serial.println("CONNECTED");

      mqttClient.publish(
        MQTT_TOPIC,
        "{\"gateway\":\"KAVACHAM_GATEWAY\",\"status\":\"online\"}"
      );
    }
    else
    {
      Serial.print("FAILED: ");
      Serial.println(
        mqttClient.state()
      );

      delay(2000);
    }
  }
}

// =====================================================
// BLE RECEIVE
// =====================================================

class ScanCallbacks :
  public NimBLEScanCallbacks
{
  void onResult(
    const NimBLEAdvertisedDevice *device)
    override
  {
    if (!device->haveName())
      return;

    String name =
      device->getName().c_str();

    if (name != "KAVACHAM")
      return;

    if (!device->haveManufacturerData())
      return;

    std::string data =
      device->getManufacturerData();

    // EXACTLY 11 BYTES
    if (data.length() != 11)
      return;

    uint8_t packet[11];

    memcpy(
      packet,
      data.data(),
      11
    );

    // Check checksum
    if (
      checksum(packet) != packet[10]
    )
    {
      Serial.println(
        "Invalid checksum"
      );

      return;
    }

    // -------------------------------------------------
    // Decode
    // -------------------------------------------------

    uint8_t workerID = packet[0];
    uint8_t type = packet[1];
    uint8_t sequence = packet[2];

    uint16_t gas =
      ((uint16_t)packet[3] << 8) |
      packet[4];

    uint8_t temperature = packet[5];

    float acceleration =
      packet[6] / 10.0;

    uint8_t orientation = packet[7];
    uint8_t movement = packet[8];

    uint8_t hop = packet[9];

    int rssi =
      device->getRSSI();

    String address =
      device->getAddress().toString().c_str();

    // -------------------------------------------------
    // Print
    // -------------------------------------------------

    Serial.println();
    Serial.println("================================");
    Serial.println("KAVACHAM GATEWAY");
    Serial.println("================================");

    Serial.print("WORKER: ");
    Serial.println(workerID);

    Serial.print("MESSAGE: ");
    Serial.println(messageName(type));

    Serial.print("SEQUENCE: ");
    Serial.println(sequence);

    Serial.print("GAS: ");
    Serial.println(gas);

    Serial.print("TEMPERATURE: ");
    Serial.println(temperature);

    Serial.print("ACCELERATION: ");
    Serial.println(acceleration);

    Serial.print("ORIENTATION: ");
    Serial.println(orientation);

    Serial.print("MOVEMENT: ");
    Serial.println(movement);

    Serial.print("HOP: ");
    Serial.println(hop);

    Serial.print("RSSI: ");
    Serial.println(rssi);

    Serial.print("BLE ADDRESS: ");
    Serial.println(address);

    Serial.println("================================");

    // -------------------------------------------------
    // MQTT JSON
    // -------------------------------------------------

    String json = "{";

    json += "\"worker_id\":\"WSN-";
    json += workerID;
    json += "\",";

    json += "\"worker\":";
    json += workerID;
    json += ",";

    json += "\"message\":\"";
    json += messageName(type);
    json += "\",";

    json += "\"sequence\":";
    json += sequence;
    json += ",";

    json += "\"gas\":";
    json += gas;
    json += ",";

    json += "\"temperature\":";
    json += temperature;
    json += ",";

    json += "\"acceleration\":";
    json += acceleration;
    json += ",";

    json += "\"orientation\":";
    json += orientation;
    json += ",";

    json += "\"movement\":";
    json += movement;
    json += ",";

    json += "\"hop\":";
    json += hop;
    json += ",";

    json += "\"rssi\":";
    json += rssi;
    json += ",";

    json += "\"ble_address\":\"";
    json += address;
    json += "\",";

    json += "\"gateway\":\"KAVACHAM_GATEWAY\"";

    json += "}";

    // -------------------------------------------------
    // MQTT
    // -------------------------------------------------

    if (mqttClient.connected())
    {
      if (
        mqttClient.publish(
          MQTT_TOPIC,
          json.c_str()
        )
      )
      {
        Serial.println(
          "MQTT PUBLISHED"
        );
      }
      else
      {
        Serial.println(
          "MQTT PUBLISH FAILED"
        );
      }
    }
  }
};

ScanCallbacks callbacks;

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("     KAVACHAM GATEWAY");
  Serial.println(" BLE -> MQTT -> MOSQUITTO");
  Serial.println("================================");

  connectWiFi();

  mqttClient.setServer(
    MQTT_BROKER,
    MQTT_PORT
  );

  // MQTT JSON can be larger than BLE packet
  mqttClient.setBufferSize(512);

  NimBLEDevice::init(
    "KAVACHAM_GATEWAY"
  );

  bleScan =
    NimBLEDevice::getScan();

  bleScan->setScanCallbacks(
    &callbacks
  );

  bleScan->setActiveScan(true);

  bleScan->setInterval(20);
  bleScan->setWindow(20);

  bleScan->start(
    0,
    false
  );

  Serial.println(
    "BLE SCANNER ACTIVE"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  if (
    WiFi.status() != WL_CONNECTED
  )
  {
    connectWiFi();
  }

  if (!mqttClient.connected())
  {
    connectMQTT();
  }

  mqttClient.loop();
  // Send test message every 5 seconds
  static unsigned long lastPublish = 0;

  if (millis() - lastPublish >= 5000) {

    lastPublish = millis();

    String message = "Hello from ESP32 Gateway";

    if (mqttClient.publish(MQTT_TOPIC, message.c_str())) {
      Serial.println("MQTT message published!");
    } else {
      Serial.println("MQTT publish FAILED!");
    }}

  delay(10);
}
