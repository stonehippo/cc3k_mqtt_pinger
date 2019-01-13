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
