#include <MQTT.h>
#include "debug_config.h"
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

// turn on additional debug via serial port
// #ifndef MQTT_DEBUG
// #define MQTT_DEBUG
// #endif

// set defaults for client name and ping interval
#ifndef MQTT_CLIENT_NAME
#define MQTT_CLIENT_NAME   "cc3k_pinger"
#endif

#ifndef MQTT_PING_INTERVAL
#define MQTT_PING_INTERVAL 5000L
#endif

// enable adding geolocation to MQTT payload
// #ifndef ENABLE_GEO
// #define ENABLE_GEO
// #endif

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

// Oversampling for the ADC, 2 bits by default
#ifndef OVERSAMPLE_BITS
#define OVERSAMPLE_BITS	2
#endif

long timerInterval = 0;
char lat[10];
char lon[10];

void setup() {
  Serial.begin(115200);
  wifiConnect();
  #ifdef ENABLE_GEO
  char geo[40];
  geo[0] = '\0';
  getGeolocation(geo);
  
  char toss[1]; // throwaway for the delimiter
  sscanf(geo, "%[^,]%[,]%s", lat, toss, lon);
  
  out(F("Geolocation: "));
  out(lat);
  out(F(","));
  message(lon);
  #endif
  client.begin(MQTT_BROKER, conn);
  connectToBroker();
  sendPayload("{'status': 'connected to broker'}");
}

void loop() {
  client.loop();
  // ping once per interval defined in config, using a non-blocking timer
  if (timerInterval == 0) {
    startTimer(timerInterval);
  }
  if (isTimerExpired(timerInterval, MQTT_PING_INTERVAL)) {
    clearTimer(timerInterval); // reset for the next interval
    /*
      Attempt to reconnect
      
      previously, this firmware checked to see if the CC3000 was 
      still connected first. However, it appears that there's a
      possible bug in the library. After running for several hours
      the device is not longer able to connect to the broker, and
      begins trying to reconnect. However the logic that was supposed
      to detect disconnection from Wifi never gets triggered.
      
      To hack around this, if the broker connection breaks, this code
      assumes that it's CC3000 connection with the AP as the root
      cause, and tries to resolve it by restarting the network
      stack before trying the MQTT broker again.
    */
    if (!client.connected()) {
      cc3k.reboot(); // kick the CC3000, hoping to get it going again
      wifiConnect(); // attempt to reconnect to the AP      
      connectToBroker(); // reconnect with the MQTT broker
      sendPayload("{'status': 'reconnected to broker'}");
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
  #ifdef ENABLE_GEO
  payload += ", \"lat\": ";
  payload += lat;
  payload += ", \"lon\": ";
  payload += lon;
  payload += ", \"ele\": 0";
  #endif
  payload += "}";
  sendPayload(payload);
  #ifdef MQTT_DEBUG
  out(F("Sent payload: "));
  message(payload);
  #endif
}

int readSensor() {
  /*
    Oversample the ADC
    
    The ADC is a 10-bit device, but we can get a higher precision via oversampling.
    This lets us smooth out noise from the sensor, and for every 2^2^n samples we take,
    then dividing by 2^n (dropping two bits), it increases the precision of the reading by n bits.
    
    For example, taking 4 10-bit samples (2^2^1) and partial averaging them gives a virtual 
    11 bit result, and 16 samples (2^2^2) gives virtual 12 bit precision.
    
    The downside of oversampling is speed: because you're doing successive reads of the ADC,
    it will be at least 1/n slower than a single read.
    
    Given that this firmware is meant to broadcast to an MQTT broker and data service that
    is likely to rate limiting (for example, Adafruit IO restricts all incoming values from
    a single account to .5Hz for the free tier, and 1Hz for the paid tier), paying the price
    for the successive reads on the ADC is a fair tradeoff.
  */
  
  uint16_t sum = 0;
  byte oversampleCount = 1 << (OVERSAMPLE_BITS * 2); // 2^2^n!
  uint16_t readBuffer[oversampleCount];  
  
  for (byte i = 0; i < oversampleCount; i++) {
  	readBuffer[i] = analogRead(SENSOR_PIN);
  	#ifdef MQTT_DEBUG
  	out(F("Sample "));
  	out(i);
  	out(F(":"));
  	message(readBuffer[i]);
  	#endif
  	sum += readBuffer[i];
  }
    
  return sum/(1 << OVERSAMPLE_BITS); //
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
    while(http.connected()) {
      while(http.available()) {
        if(!http.find("HTTP/1.1 ")) {
          halt(F("Did not get a valid HTTP response"));
        } else {
          message(F("HTTP 1.1 response"));
        }
        
        int status = http.parseInt();
        if (status != 200) {
          out(F("HTTP error, got response "));
          halt(status);
        } else {
          message(F("HTTP response was 200 OK"));
        }
        
        if(!http.find("Content-Length: ")) {
          halt(F("could not determine content length"));
        }
        
        int response_length = http.parseInt();
        
        if(!http.find("\r\n\r\n")) {
          halt(F("could not find HTTP response body"));
        } else {
          message(F("Processing response body"));
        }
        
        http.readBytes(response, response_length);
        response[response_length] = '\0';
        http.close();
      }
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
