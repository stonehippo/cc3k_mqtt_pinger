#include <Arduino.h>
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

void wifiConnect();
void connectToBroker();
void sendStatusToBroker();
void pingBroker();
