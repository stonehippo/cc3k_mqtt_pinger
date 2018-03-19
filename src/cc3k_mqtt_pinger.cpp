#include <MQTT.h>
#include "cc3k_mqtt_pinger.h"
#include "wifi_config.h"
#include "mqtt_config.h"

#define Adafruit_CC3000_IRQ   3
#define Adafruit_CC3000_VBAT  5
#define Adafruit_CC3000_CS    10

Adafruit_CC3000 cc3k = Adafruit_CC3000(Adafruit_CC3000_CS, Adafruit_CC3000_IRQ, Adafruit_CC3000_VBAT, SPI_CLOCK_DIVIDER);
Adafruit_CC3000_Client conn;
MQTTClient client;

// #define MQTT_DEBUG

#define IDLE_TIMEOUT_MS  3000

#define out(m) {Serial.print(m); }
#define message(m) { Serial.println(m); }
#define halt(err) { message(err); while(1); }

uint32_t incrementer = 0;

void setup() {
  Serial.begin(115200);
  wifiConnect();
  client.begin(MQTT_BROKER, conn);
  connectToBroker();
}

void loop() {
  client.loop();
  if (!client.connected()) {
    connectToBroker();
  } else {
    pingBroker();
  }
  delay(5000); // ping once every 5 seconds
}

void wifiConnect() {
  message("Initializing CC3000...");
  if (!cc3k.begin()) {
    halt(F("CC3000 did not initialize"));
  } else {
    message(F("CC3000 initialized"));
  }

  out(F("Connecting to AP "));
  message(F(WLAN_SSID));
  if (!cc3k.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    halt(F("Could not connect to AP"));
  } else {
    message(F("Connected to AP"));
  }

  message(F("Requesting DHCP..."));
  while (!cc3k.checkDHCP()) {
    message(F("DHCP not set. Retrying..."));
    delay(100);
  }

  message(F("Wifi setup complete"));
}

void connectToBroker() {
  while(!client.connect("pinger", MQTT_USER, MQTT_KEY)) {
    out(".");
    delay(1000);
  }

  message("Connected to MQTT broker!");
}

void pingBroker() {
  incrementer = incrementer + 1;
  String payload = "{'ping':'pong!', 'count':";
  payload += incrementer;
  payload += "}";
  client.publish(MQTT_TOPIC, payload);
}
