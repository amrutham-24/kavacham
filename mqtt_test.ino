#include <WiFi.h>
#include <PubSubClient.h>

// =========================
// Wi-Fi
// =========================
const char* WIFI_SSID = "Megha";
const char* WIFI_PASSWORD = "megha1356";

// =========================
// MQTT
// =========================
// Use the IP address of the computer/Raspberry Pi
// running Mosquitto.
// DO NOT use localhost or 127.0.0.1
const char* MQTT_BROKER = "10.136.236.22";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "mine/test";

WiFiClient espClient;
PubSubClient mqttClient(espClient);


// =========================
// Wi-Fi connection
// =========================
void connectWiFi() {

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}


// =========================
// MQTT connection
// =========================
void connectMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Connecting to MQTT... ");

    String clientID = "ESP32-Gateway-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientID.c_str())) {

      Serial.println("CONNECTED!");

      // Test message immediately after connection
      mqttClient.publish(MQTT_TOPIC, "ESP32 Gateway connected");

    } else {

      Serial.print("FAILED, error code = ");
      Serial.println(mqttClient.state());

      Serial.println("Retrying in 2 seconds...");
      delay(2000);
    }
  }
}


// =========================
// Setup
// =========================
void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 MQTT Gateway Test");
  Serial.println("================================");

  connectWiFi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
}


// =========================
// Main loop
// =========================
void loop() {

  // Reconnect Wi-Fi if necessary
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Reconnect MQTT if necessary
  if (!mqttClient.connected()) {
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
    }
  }
}
