# CC3000 MQTT pinger

A very basic device that uses a TI CC3000 to communicate with an MQTT broker over Wifi.

Has been tested with:

- [Shiftr.io](https://shiftr.io) - MQTT feed works, used the HTTP interface prior to commit 1e0e008
- [Adafruit IO](https://io.adafruit.com) - MQTT feeds work


## Developer's Notes

When I first designed this, I tried to use the [Adafruit MQTT library](https://github.com/adafruit/Adafruit_MQTT_Library). Unfortunately, they discontinued support for the CC3000. This is understandable, since it's not a great SoC, but I still have a couple of them around that I wanted to use. I tried an older version of the library that should have worked, but it wasn't very stable, and I kept having issues connecting. I've found that this library works well with other supported chips, like the ESP8266, so I was sad that it didn't work out here.

The first successful implementation came when I switched to the [Shiftr](https://shiftr.io) HTTP interface. This let me write HTTP directly using a JSON packet. That was fine, it worked, but it kind of left me wanting, in part because it was a little slow setting up an killing the HTTP connection, and in part because I wanted to use MQTT from the outset.

I've since refactored the code to use [arduino-mqtt](https://github.com/256dpi/arduino-mqtt). Not only is this library stable, it's compatible with any Arduino networking library that uses the `Client` interface. This means that I can port the pinger to other chips with ease. Hooray!