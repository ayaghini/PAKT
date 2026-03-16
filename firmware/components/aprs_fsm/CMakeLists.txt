idf_component_register(
    SRCS "TxScheduler.cpp"
          "TxResultEncoder.cpp"
          "AprsTaskContext.cpp"
    INCLUDE_DIRS "include"
    REQUIRES aprs esp_system esp_timer freertos payload_codec
)
