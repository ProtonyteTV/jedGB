cmake_minimum_required(VERSION 3.20)
project(jedGB CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

find_package(SDL2 REQUIRED)

set(SOURCES
    src/main.cpp
    src/bus.cpp
    src/cpu.cpp
    src/ppu.cpp
    src/cartridge.cpp
    src/apu.cpp
    src/jedgb_api.cpp
    src/tinyfiledialogs.c
)

add_executable(jedGB ${SOURCES})

target_include_directories(jedGB PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${SDL2_INCLUDE_DIRS}
)

if(WIN32)
    target_link_libraries(jedGB PRIVATE comdlg32)
    if(TARGET SDL2::SDL2-static)
        target_link_libraries(jedGB PRIVATE SDL2::SDL2-static)
    elseif(TARGET SDL2::SDL2)
        target_link_libraries(jedGB PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(jedGB PRIVATE ${SDL2_LIBRARIES})
    endif()

    if(TARGET SDL2::SDL2main)
        target_link_libraries(jedGB PRIVATE SDL2::SDL2main)
    endif()
else()
    if(TARGET SDL2::SDL2)
        target_link_libraries(jedGB PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(jedGB PRIVATE ${SDL2_LIBRARIES})
    endif()
endif()