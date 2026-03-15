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
    # Component sources compiled directly (no ESP-IDF build system needed)
    ${AX25_SRC}
    ${APRS_SRC}
    ${MODEM_SRC}
    ${BLE_SRC}
    ${FSM_SRC}
    ${TELEM_SRC}
    ${CAP_SRC}
    ${GPS_SRC}
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
    ${doctest_SOURCE_DIR}/doctest      # provides <doctest/doctest.h>
)

target_compile_options(pakt_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

# CTest integration
include(CTest)
add_test(NAME pakt_tests COMMAND pakt_tests --reporters=console --no-intro)
