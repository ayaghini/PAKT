idf_component_register(
    SRCS "Sa818CommandFormatter.cpp"
         "Sa818ResponseParser.cpp"
         "Sa818Radio.cpp"
    INCLUDE_DIRS "include"
    REQUIRES pakt_hal
)
