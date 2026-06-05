if(NOT RUNNING_FROM_SCRIPT)
    message(FATAL_ERROR "❌ Do not run cmake directly! Please use './build.sh' instead.")
endif()

cmake_minimum_required(VERSION 3.11)
project(VEDA_Linux_Project)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED on)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
set(THIRDPARTY_INC ${THIRDPARTY_DIR}/include)
set(THIRDPARTY_LIB ${THIRDPARTY_DIR}/lib)

link_directories(${THIRDPARTY_LIB} ${THIRDPARTY_DIR})
include_directories(${THIRDPARTY_INC} ${CMAKE_CURRENT_SOURCE_DIR}/include)

# -------------------------------------------------------------
# 🛠️ 공유 라이브러리 (.so) 타깃 등록
# -------------------------------------------------------------
add_library(led SHARED src/devices/led.c)
target_link_libraries(led wiringPi crypt)

add_library(buzzer SHARED src/devices/buzzer.c)
target_link_libraries(buzzer wiringPi crypt)

add_library(cds SHARED src/devices/cds.c)
target_link_libraries(cds wiringPi crypt)

add_library(segment SHARED src/devices/segment.c)
target_link_libraries(segment wiringPi crypt)

# -------------------------------------------------------------
# 🚀 실행 파일 (Server) 타깃 등록
# -------------------------------------------------------------
add_executable(server
    src/server/main.c
    src/server/config.c
    src/server/network_server.c
    src/server/device_manager.c
    
    src/server/handlers/buzzer_handler.c
    src/server/handlers/cds_handler.c
    src/server/handlers/led_handler.c
    src/server/handlers/segment_handler.c
)

target_link_libraries(server dl pthread crypt wiringPi)

set_target_properties(server PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN:$ORIGIN/lib"
)

# -------------------------------------------------------------
# 🚀 실행 파일 (Client) 타깃 등록
# -------------------------------------------------------------
add_executable(client
    src/client/client_main.c
)
target_link_libraries(client pthread)

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)