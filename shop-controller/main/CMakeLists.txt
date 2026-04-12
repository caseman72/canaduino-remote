idf_component_register(
    SRCS "main.c" "wifi.c" "mqtt.c" "zigbee.c" "relay.c" "temp.c" "ota.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "mqtt_ca.pem"
)
