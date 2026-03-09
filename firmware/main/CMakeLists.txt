idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES pakt_hal ax25 aprs modem ble_services esp_system esp_timer freertos
)
