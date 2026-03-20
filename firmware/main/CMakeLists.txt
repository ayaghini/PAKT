idf_component_register(
    SRCS
        "main.cpp"
        "audio_bench_test/audio_bench_test.cpp"
        "sa818_bench_test/sa818_bench_test.cpp"
    INCLUDE_DIRS
        "."
        "audio_bench_test"
        "sa818_bench_test"
    REQUIRES pakt_hal ax25 aprs modem ble_services aprs_fsm payload_codec telemetry capability gps safety_watchdog radio_sa818 kiss nvs_flash esp_system esp_timer esp_psram freertos driver
)
