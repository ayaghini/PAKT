# PAKT host unit tests
#
# Standalone CMake project – runs on any desktop OS (Windows, Linux, macOS).
# Does NOT depend on ESP-IDF; tests pure-software components only.
#
# Build:
#   cmake -S firmware/test_host -B build/test_host -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/test_host
#   ./build/test_host/pakt_tests

cmake_minimum_required(VERSION 3.16)
project(pakt_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── doctest (header-only, fetched at configure time) ─────────────────────────
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

# ── Component source and include paths ───────────────────────────────────────
set(COMP "${CMAKE_CURRENT_SOURCE_DIR}/../components")

set(PAKT_HAL_INCLUDE "${COMP}/pakt_hal/include")
set(PAKT_HAL_MOCK    "${COMP}/pakt_hal/mock")
set(AX25_INCLUDE     "${COMP}/ax25/include")
set(AX25_SRC         "${COMP}/ax25/Ax25.cpp")
set(APRS_INCLUDE     "${COMP}/aprs/include")
set(APRS_SRC         "${COMP}/aprs/Aprs.cpp")
set(MODEM_INCLUDE    "${COMP}/modem/include")
set(MODEM_SRC        "${COMP}/modem/AfskModulator.cpp"
                     "${COMP}/modem/AfskDemodulator.cpp")
set(BLE_INCLUDE      "${COMP}/ble_services/include")
set(BLE_SRC          "${COMP}/ble_services/BleChunker.cpp")
set(FSM_INCLUDE      "${COMP}/aprs_fsm/include")
set(FSM_SRC          "${COMP}/aprs_fsm/TxScheduler.cpp")
set(TELEM_INCLUDE    "${COMP}/telemetry/include")
set(TELEM_SRC        "${COMP}/telemetry/Telemetry.cpp")
set(CAP_INCLUDE      "${COMP}/capability/include")
set(CAP_SRC          "${COMP}/capability/DeviceCapabilities.cpp")
set(GPS_INCLUDE      "${COMP}/gps/include")
set(GPS_SRC          "${COMP}/gps/NmeaParser.cpp")
set(CODEC_INCLUDE    "${COMP}/payload_codec/include")
set(CODEC_SRC        "${COMP}/payload_codec/PayloadValidator.cpp")
set(FSM_EXTRA_SRC    "${COMP}/aprs_fsm/TxResultEncoder.cpp"
                     "${COMP}/aprs_fsm/AprsTaskContext.cpp")
set(WATCHDOG_INCLUDE "${COMP}/safety_watchdog/include")
set(WATCHDOG_SRC     "${COMP}/safety_watchdog/PttWatchdog.cpp"
                     "${COMP}/safety_watchdog/PttController.cpp")
set(SA818_INCLUDE    "${COMP}/radio_sa818/include")
set(SA818_SRC        "${COMP}/radio_sa818/Sa818CommandFormatter.cpp"
                     "${COMP}/radio_sa818/Sa818ResponseParser.cpp"
                     "${COMP}/radio_sa818/Sa818Radio.cpp")
set(KISS_INCLUDE     "${COMP}/kiss/include")
set(KISS_SRC         "${COMP}/kiss/KissFramer.cpp")

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(pakt_tests
    test_main.cpp
    test_hal_mocks.cpp
    test_ax25.cpp
    test_aprs.cpp
    test_afsk_modem.cpp
    test_ble_chunker.cpp
    test_tx_scheduler.cpp
    test_telemetry.cpp
    test_capability.cpp
    test_nmea_parser.cpp
    test_payload_validator.cpp
    test_tx_integration.cpp
    test_ptt_watchdog.cpp
    test_config_store.cpp
    test_sa818.cpp
    test_kiss_framer.cpp
    # Component sources compiled directly (no ESP-IDF build system needed)
    ${AX25_SRC}
    ${APRS_SRC}
    ${MODEM_SRC}
    ${BLE_SRC}
    ${FSM_SRC}
    ${FSM_EXTRA_SRC}
    ${TELEM_SRC}
    ${CAP_SRC}
    ${GPS_SRC}
    ${CODEC_SRC}
    ${WATCHDOG_SRC}
    ${SA818_SRC}
    ${KISS_SRC}
)

target_include_directories(pakt_tests PRIVATE
    ${PAKT_HAL_INCLUDE}
    ${PAKT_HAL_MOCK}
    ${AX25_INCLUDE}
    ${APRS_INCLUDE}
    ${MODEM_INCLUDE}
    ${BLE_INCLUDE}
    ${FSM_INCLUDE}
    ${TELEM_INCLUDE}
    ${CAP_INCLUDE}
    ${GPS_INCLUDE}
    ${CODEC_INCLUDE}
    ${WATCHDOG_INCLUDE}
    ${SA818_INCLUDE}
    ${KISS_INCLUDE}
    ${doctest_SOURCE_DIR}              # provides <doctest/doctest.h>
    ${doctest_SOURCE_DIR}/doctest      # provides <doctest.h> (fallback)
)

target_compile_options(pakt_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

# CTest integration
include(CTest)
add_test(NAME pakt_tests COMMAND pakt_tests --reporters=console --no-intro)
