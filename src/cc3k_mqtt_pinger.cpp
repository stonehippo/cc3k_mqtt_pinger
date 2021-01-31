#include <MQTT.h>
#include "cc3k_mqtt_pinger.h"
#include "wifi_config.h"
#include "mqtt_config.h"
#include "TimingHelpers.h"

#define Adafruit_CC3000_IRQ   3
#define Adafruit_CC3000_VBAT  5
#define Adafruit_CC3000_CS    10

Adafruit_CC3000 cc3k = Adafruit_CC3000(Adafruit_CC3000_CS, Adafruit_CC3000_IRQ, Adafruit_CC3000_VBAT, SPI_CLOCK_DIVIDER);
Adafruit_CC3000_Client conn;
MQTTClient client;

// #define MQTT_DEBUG

// set defaults for client name and ping interval
#ifndef MQTT_CLIENT_NAME
#define MQTT_CLIENT_NAME   "cc3k_pinger"
#endif

#ifndef MQTT_PING_INTERVAL
#define MQTT_PING_INTERVAL 5000L
#endif

// pin to read sensor data from
#ifndef SENSOR_PIN
#define SENSOR_PIN A0
#endif

// IP geolocation providers
#ifndef GEO_LOOKUP_PROVIDER
#define GEO_LOOKUP_PROVIDER "ip-api.com"
#endif

// some helper macros for output
#define out(m) { Serial.print(m); }
#define message(m) { Serial.println(m); }
#define halt(err) { message(err); while(1); }

// HTTP helper macros
#define httpRequestPart(part) { http.fastrprint(part); }
#define httpNewline() { httpRequestPart("\r\n"); }
#define httpGetMethod() { httpRequestPart("GET "); }
#define httpProtocol() { httpRequestPart(" HTTP/1.1"); httpNewline(); }
#define httpHostHeader(hostname) { httpRequestPart("Host: "); httpRequestPart(hostname); httpNewline(); }
#define httpEndRequest() { httpNewline(); }

#ifndef HTTP_TIMEOUT
#define HTTP_TIMEOUT 10000
#endif

long timerInterval = 0;
float lat = 0;
float lon = 0;

void setup() {
  Serial.begin(115200);
  wifiConnect();
  char geo[40];
  geo[0] = 0;
  getGeolocation(geo);
  out(F("Geolocation: "));
  message(geo);
  out(lat);
  out(F(","));
  message(lon);
  client.begin(MQTT_BROKER, conn);
  connectToBroker();
  sendStatusToBroker("{'status': 'connected to broker'}");
}

void loop() {
  bool wasWifiDisconnected = false;
  client.loop();
  // ping once per interval defined in config, using a non-blocking timer
  if (timerInterval == 0) {
    startTimer(timerInterval);
  }
  if (isTimerExpired(timerInterval, MQTT_PING_INTERVAL)) {
    clearTimer(timerInterval); // reset for the next interval
    if (!cc3k.checkConnected()) {
      client.disconnect();
      message(F("CC3000 is not connected"));
      wifiConnect();
      wasWifiDisconnected = true;
    }
    if (!client.connected()) {
      connectToBroker();
      if (wasWifiDisconnected) {
        sendStatusToBroker("{'status: 'reconnected to wifi'}");
      }
      sendStatusToBroker("{'status': 'reconnected to broker'}");
    } else {
      pingBroker();
    }
  }
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
  while(!client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_KEY)) {
    out(".");
    delay(1000);
  }
  message("Connected to MQTT broker!");
}

void sendStatusToBroker(String payload) {
  client.publish(MQTT_TOPIC, payload);
}

void pingBroker() {
  int value = readSensor();
  String payload = "{\"value\": ";
  payload += value;
  payload += ", \"lat\": ";
  payload += lat;
  payload += ", \"lon\": ";
  payload += lon;
  payload += ", \"ele\": 0";
  payload += "}";
  client.publish(MQTT_TOPIC, payload);
}

int readSensor() {
  return analogRead(SENSOR_PIN);
}

void httpGet(const char *host, const char *request, char *response) {
  uint32_t ip = 0L;
  while( ip == 0L) {
    if (!cc3k.getHostByName(host, &ip)) {
      message(F("Failed while resolving host "));
      halt(host);
    }
  }
  
  out(F("IP for host "));
  out(host);
  out(F(": "));
  cc3k.printIPdotsRev(ip);
  message(F("\n"));
  
  Adafruit_CC3000_Client http = cc3k.connectTCP(ip, 80);
  http.setTimeout(HTTP_TIMEOUT);
  if (http.connected()) {
    httpGetMethod();
    httpRequestPart(request);
    httpProtocol();
    httpHostHeader(host);
    httpEndRequest();
    
    // parse the http response
    while(http.available()) {
      if(!http.find("HTTP/1.1")) {
        halt(F("Did not get a valid HTTP response"));
      }
      
      int status = http.parseInt();
      if (status != 200) {
        out(F("HTTP error, got response "));
        halt(status);
      } else {
        message(F("Got HTTP 200 response"));
      }
      
      if(!http.find("\r\n\r\n")) {
        halt(F("could not find HTTP response body"));
      } else {
        message(F("Processing response body"));
      }
      
      http.readBytes(response, 40);
    }
  } else {
    message(F("Failed to connect to HTTP host "));
    halt(host);
  }
  message(F("\nrequest complete"));
  http.close();
}

// attempt to look up the geolocation of the device, based on the public IP
// restrict the returned fields to the data we need (latitude and longitude only),
// which cuts down on the data we have to parse
void getGeolocation(char *response) {
  String query = "/csv/";
  query += "?fields=lat,lon";
  httpGet(GEO_LOOKUP_PROVIDER, query.c_str(), response);
}