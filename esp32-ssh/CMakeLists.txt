cmake_minimum_required(VERSION 3.10)
project(esp32-ssh CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  message(FATAL_ERROR "esp32-ssh supports Windows and Linux only, not macOS.")
endif()

add_executable(esp32-ssh
  src/main.cpp
  src/sha256.cpp
)

target_include_directories(esp32-ssh PRIVATE src)

if(WIN32)
  target_link_libraries(esp32-ssh ws2_32)
endif()
