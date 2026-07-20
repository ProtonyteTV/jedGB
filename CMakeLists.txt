cmake_minimum_required(VERSION 3.20)
project(jedGB CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

include_directories(include ${SDL2_INCLUDE_DIRS})

set(SOURCES
    src/main.cpp
    src/bus.cpp
    src/cpu.cpp
    src/ppu.cpp
    src/cartridge.cpp
    src/apu.cpp
)

add_executable(jedGB ${SOURCES})

target_link_libraries(jedGB PRIVATE ${SDL2_LIBRARIES})