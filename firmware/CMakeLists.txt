# Track Robot Firmware - ESP-IDF Project CMake
cmake_minimum_required(VERSION 3.16)

# Set the target chip - Heltec WiFi Kit 32 V3 (ESP32-S3)
set(IDF_TARGET "esp32s3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(track-robot)
