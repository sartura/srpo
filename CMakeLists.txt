cmake_minimum_required(VERSION 2.8)
project(srpo C)

include(GNUInstallDirs)
include(CompileOptions.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(UCI_CONFIG_DIR "/etc/config" CACHE STRING "Path to UCI config directory")

add_definitions("-DSRPO_UCI_CONFIG_DIR=\"${UCI_CONFIG_DIR}\"")

include_directories(${CMAKE_SOURCE_DIR}/src/)

set(SOURCES
    src/srpo_ubus.c
    src/srpo_uci.c
    src/utils/memory.c
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

find_package(SYSREPO REQUIRED)
find_package(UCI REQUIRED)
find_package(LIBUBOX REQUIRED)
find_package(LIBUBUS REQUIRED)

target_link_libraries(
    ${PROJECT_NAME}
    ${SYSREPO_LIBRARIES}
    ${UCI_LIBRARIES}
    ${LIBUBOX_LIBRARIES}
    ${LIBUBUS_LIBRARIES}
)

include_directories(
    ${SYSREPO_INCLUDE_DIRS}
    ${UCI_INCLUDE_DIR}
    ${LIBUBOX_INCLUDE_DIR}
    ${LIBUBUS_INCLUDE_DIR}
)

# installation
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${PROJECT_SOURCE_DIR}/src/srpo_ubus.h ${PROJECT_SOURCE_DIR}/src/srpo_uci.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

