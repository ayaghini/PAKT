# PAKT Firmware – ESP-IDF top-level project
#
# Toolchain : ESP-IDF v5.3.2
# Target    : ESP32-S3
# Setup     : https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/get-started/
# Build     : idf.py -t esp32s3 build
# Flash     : idf.py -p <PORT> flash monitor

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(pakt_firmware VERSION 0.1.0)
