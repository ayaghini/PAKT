# pakt_hal – hardware abstraction layer (header-only for interfaces)
# Concrete drivers (SA818, SGTL5000, NVS) are separate components that
# implement these interfaces and register themselves with the DI layer.
idf_component_register(
    INCLUDE_DIRS "include"
)
