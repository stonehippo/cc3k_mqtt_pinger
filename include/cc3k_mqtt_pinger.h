#include <Arduino.h>
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

void wifiConnect();
void connectToBroker();
void sendStatusToBroker(String payload);
void pingBroker();
int readSensor();
void httpGet(const char *host, const char *request);
void getGateway();
void getGeolocation();