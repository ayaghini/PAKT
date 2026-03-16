idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES pakt_hal ax25 aprs modem ble_services aprs_fsm payload_codec telemetry capability gps safety_watchdog radio_sa818 nvs_flash esp_system esp_timer freertos driver
)
