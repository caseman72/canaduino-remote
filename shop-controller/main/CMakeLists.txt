idf_component_register(
    SRCS "main.c" "wifi.c" "mqtt.c" "zigbee.c" "relay.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "mqtt_ca.pem"
)
