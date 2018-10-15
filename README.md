# CC3000 MQTT pinger

A very basic device that uses a TI CC3000 to communicate with an MQTT broker over Wifi.

Has been tested with:

- [Shiftr.io](https://shiftr.io) - MQTT feed works, used the HTTP interface prior to commit 1e0e008
- [Adafruit IO](https://io.adafruit.com) - MQTT feeds work

## Build

You'll need [Platform.io](https://platformio.org). You might want to run it inside of Visual Studio Code, as I do, or another editor supported by Platform.io.

```
$ git clone https://github.com/stonehippo/cc3k_mqtt_pinger.git
$ cd cc3k_mqtt_pinger
$ pio run 
```

## Configuration

There are a couple of files that you'll need to create to build this firmware correctly.

### wifi_config.h

You'll need to put your WiFi connection info in `src/wifi_config.h`.

```
#define WLAN_SSID       "...your SSID..."
#define WLAN_PASS       "...you network password..."
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2
```


An example version can be found in [src/example_wifi_config.h](src/example_wifi_config.h)

### mqtt_config.h

You'll need to put your MQTT broker URL, access key, and topic into `src/mqtt_config.h`. You can also override the default client name and ping interval.

```
#define MQTT_BROKER "broker.shiftr.io"
// User portion of a shiftr.io or other MQTT broker full access token
#define MQTT_USER   "...shiftr.io token user..."
// Key portion of the same shiftr.io or other MQTT broker full access token
#define MQTT_KEY    "...shiftr.io token key..."
#define MQTT_TOPIC  "feeds/ping"
// Name of the client as shown in MQTT feed
#define MQTT_CLIENT_NAME   "cc3k_pinger"
// Interval between pings, in milliseconds
#define MQTT_PING_INTERVAL 60000L
```

An example version can be found in [src/example_mqtt_config.h](src/example_mqtt_config.h)

## Developer's Notes

When I first designed this, I tried to use the [Adafruit MQTT library](https://github.com/adafruit/Adafruit_MQTT_Library). Unfortunately, they discontinued support for the CC3000. This is understandable, since it's not a great SoC, but I still have a couple of them around that I wanted to use. I tried an older version of the library that should have worked, but it wasn't very stable, and I kept having issues connecting. I've found that this library works well with other supported chips, like the ESP8266, so I was sad that it didn't work out here.

The first successful implementation came when I switched to the [Shiftr](https://shiftr.io) HTTP interface. This let me write HTTP directly using a JSON packet. That was fine, it worked, but it kind of left me wanting, in part because it was a little slow setting up and killing the HTTP connection, and in part because I wanted to use MQTT from the outset. Using the HTTP interface of a given service meant the implementation was tied to that API. Yuck.

I've since refactored the code to use [arduino-mqtt](https://github.com/256dpi/arduino-mqtt). Not only is this library stable, it's compatible with any Arduino networking library that uses the `Client` interface. This means that I can port the pinger to other chips with ease. Hooray!

### Connecting to an MQTT broker via IP address does not work

For reasons I haven't yet figured out, it's not possible to connect to an MQTT broker via IP address. The problem isn't in the `arduino-mqtt` library, since it works fine when I've tested it with other WiFi SoCs like the ESP8266 and an IP address (for example, when connecting to a local Mosquitto broker).

I think the issue is in the CC3K driver. I don't think it's smart enough to figure out the resolution on its own, or its API is slightly off compared to `Client`. In my HTTP-based implementation, I was manually handling name resolution with a function called `resolveBrokerIP`, but I killed that when I switched over to the MQTT implementation.

I'll try to figure that out some time, but for now, this firmware will only work with Internet-resolvable hostnames.