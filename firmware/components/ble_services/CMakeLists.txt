idf_component_register(
    SRCS "BleChunker.cpp"
         "BleServer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_system esp_timer freertos pakt_hal
)
