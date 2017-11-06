#include "cc3k_shiftr_pinger.h"
#include "wifi_config.h"
#include "shiftr_config.h"
#include "Base64.h"

#define Adafruit_CC3000_IRQ   3
#define Adafruit_CC3000_VBAT  5
#define Adafruit_CC3000_CS    10

Adafruit_CC3000 cc3k = Adafruit_CC3000(Adafruit_CC3000_CS, Adafruit_CC3000_IRQ, Adafruit_CC3000_VBAT, SPI_CLOCK_DIVIDER);
Adafruit_CC3000_Client conn;

// #define SHIFTR_DEBUG

#define IDLE_TIMEOUT_MS  3000

#define out(m) {Serial.print(m); }
#define message(m) { Serial.println(m); }
#define halt(err) { message(err); while(1); }

#ifndef SHIFTR_DEBUG
#define request(s) { conn.fastrprint(s); }
#endif

#ifdef SHIFTR_DEBUG
#define request(s) { Serial.print(s); }
#endif

uint32_t brokerIPAddress = 0;
char inputString[] = SHIFTR_USER ":" SHIFTR_KEY;
int inputLength = sizeof(inputString) - 1;
int encodedLength = Base64.encodedLength(inputLength);
char brokerAuth[64];

uint32_t incrementer = 0;

void setup() {
  Serial.begin(115200);
  wifiConnect();
  while (!resolveBrokerIP());

  Base64.encode(brokerAuth, inputString, inputLength);
  message(brokerAuth);
}

void loop() {
  if (!conn.connected()) {
    connectToBroker();
  } else {
    pingBroker();
    unsigned long lastRead = millis();
    while (conn.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
      while (conn.available()) {
        char c = conn.read();
        Serial.print(c);
        lastRead = millis();
      }
    }
    conn.close();
    message(F("\n--------------\n"));
  }
  delay(1000);
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

bool resolveBrokerIP() {
  out(F("Resolving broker IP address: "));
  message(F(SHIFTR_BROKER));
  if (!cc3k.getHostByName(SHIFTR_BROKER, &brokerIPAddress)) {
    message(F("Could not resolve broker. Retrying..."));
    return false;
  } else {
    out(F("Broker IP address: "));
    cc3k.printIPdotsRev(brokerIPAddress);
    message(F(""));
    return true;
  }
}

bool connectToBroker() {
  conn = cc3k.connectTCP(brokerIPAddress, 80);
  if (conn.connected()) {
    message("Connected to broker");
    return true;
  } else {
    message("Could not connect to broker");
    return false;
  }
}

bool pingBroker() {
  incrementer = incrementer + 1;
  String payload = "{'ping':'pong!', 'count':";
  payload += incrementer;
  payload += "}";
  int len = payload.length();
  String payloadLen = "";
  payloadLen += len;
  if (conn.connected()) {
    request(F("POST "));
    request(SHIFTR_TOPIC);
    request(F(" HTTP/1.1\r\n"));
    request(F("Host: "));
    request(SHIFTR_BROKER);
    request(F("\r\n"));
    request(F("Authorization: Basic "));
    request(brokerAuth);
    request(F("\r\n"));
    request(F("Content-Length: "));
    request(payloadLen.c_str());
    request(F("\r\n\r\n"));
    request(payload.c_str());
    conn.println();
    message(F("Ping sent to broker"));
    return true;
  } else {
    return false;
  }
}
