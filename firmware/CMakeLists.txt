# Track Robot Firmware - ESP-IDF Project CMake
cmake_minimum_required(VERSION 3.16)

# Set the target chip
set(IDF_TARGET "esp32c5")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(track-robot)
