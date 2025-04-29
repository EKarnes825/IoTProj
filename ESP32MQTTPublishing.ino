#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_CCS811.h>
#include <ArduinoJson.h>
#include <time.h>

// Wi-Fi Configuration
const char* ssid = "Enter WiFi SSID";
const char* password = "Enter WiFi Password";

// AWS IoT Core Configuration
const char* awsEndpoint = "youramazondomain.amazonaws.com";
const int port = 8883;
const char* topic = "esp32";

// Certificates & Keys 
static const char certPem[] = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

static const char keyPem[] = R"EOF(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----
)EOF";

static const char caPem[] = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

Adafruit_CCS811 ccs;
const int FAN_PIN = 12;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// Website where we can get the time
const char* ntpServer = "pool.ntp.org";

// The setup function sets the fan to be off, looks for the CCS811 on I2C, and connects to WiFi, time, and AWS.
void setup() {
  Serial.begin(115200);

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  Wire.begin();
  if (!ccs.begin(0x5A)) {
    Serial.println("CCS811 not detected!");
    while (1);
  }
  ccs.setDriveMode(CCS811_DRIVE_MODE_10SEC);
  while (!ccs.available());

  connectToWiFi();
  
  configTime(-5 * 3600, 3600, ntpServer);  // this makes the time est with daylight savings

  waitForTime();
  connectToAWS();

  // Very important line, when the program gets a new mqtt message it will call mqttCallback automatically
  // which lets the shadow be updated from our lambda function
  mqttClient.setCallback(mqttCallback);
}

unsigned long previousSensorMillis = 0;
const long sensorInterval = 10000;  // 10 seconds to match the CCS811 drive mode

// The main loop of the program is what the ESP32 will run continously 
void loop() {
  if (!mqttClient.connected()) {
    connectToAWS();
  }
  mqttClient.loop();

  // millis lets us continuosly look for shadow updates
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    previousSensorMillis = currentMillis;

    // Grabbing sensor data
    if (ccs.available() && !ccs.readData()) {
      float eco2 = ccs.geteCO2();
      float tvoc = ccs.getTVOC();

      // Building the timestamp
      struct tm timeinfo;
      char timeString[64] = "unknown";
      if (getLocalTime(&timeinfo)) {
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
      }

      // Building the JSON payload
      StaticJsonDocument<200> doc;
      doc["eco2"] = eco2;
      doc["tvoc"] = tvoc;
      doc["timestamp"] = timeString;
      doc["deviceId"] = "esp32";

      char payload[256];
      serializeJson(doc, payload);

      // Sending the payload to AWS
      if (mqttClient.publish(topic, payload)) {
        Serial.println("Published:");
        Serial.println(payload);
      } else {
        Serial.println("Publish failed!");
      }
    }
  }

  delay(10);
}

// This function connects the ESP32 to WiFi 
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
}

// This function connects the ESP32 to AWS 
void connectToAWS() {
  wifiClient.setCACert(caPem);
  wifiClient.setCertificate(certPem);
  wifiClient.setPrivateKey(keyPem);

  mqttClient.setServer(awsEndpoint, port);
  Serial.print("Connecting to AWS IoT");

  while (!mqttClient.connect("ESP32")) {
    Serial.print(".");
    delay(100);
  }

  // subscribe with QOS 1 because we need to see these messages
  mqttClient.subscribe("$aws/things/ESP32/shadow/update/delta", 1);
  Serial.println("\nAWS Connected!");
}

// This function grabs the time from ntp
void waitForTime() {
  struct tm timeinfo;
  Serial.print("Waiting for NTP time");
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime acquired!");
}

// This function, called when any mqtt message is recieved, scans the message for
// the "ventilation" phrase which indicated a new device shadow state
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);
  
  // scanning the JSON payload to see if "ventilation" is updated and writing the value 0 or 1 to the pin
  if (doc.containsKey("state") && doc["state"].containsKey("ventilation")) {
    int fanState = doc["state"]["ventilation"];
    digitalWrite(FAN_PIN, fanState);
    
    updateReportedState(fanState);
    
    Serial.print("Fan set to: ");
    Serial.println(fanState);
  }
}

// This function updates the reported state in AWS
void updateReportedState(int state) {
  StaticJsonDocument<200> doc;
  doc["state"]["reported"]["ventilation"] = state;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  
  mqttClient.publish("$aws/things/ESP32/shadow/update", jsonBuffer);
}