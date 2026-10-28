include(FetchContent)

cmake_minimum_required(VERSION 3.22)

project(YomitanMPV)

option(COPY_BUILD_TO_MPV_SCRIPTS "Copy build to ~/.config/mpv/scripts" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_NAME "YomitanMPV")
set(PROJECT_AUTHOR "konekowo")
set(PROJECT_VERSION "0.0.1")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED mpv)
find_package(spdlog REQUIRED)
find_package(CURL REQUIRED)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

configure_file(
        src/build_info.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/src/build_info.h
        @ONLY
)

add_library(YomitanMPV SHARED
    src/main.cpp
    src/main.h
        src/util/util.cpp
        src/util/util.h
        src/util/config.cpp
        src/util/config.h
)

target_include_directories(YomitanMPV PRIVATE
    ${MPV_INCLUDE_DIRS}
    src
    ${CMAKE_CURRENT_BINARY_DIR}/generated/src
)
target_link_libraries(YomitanMPV PRIVATE
        ${MPV_LIBRARIES}
        spdlog::spdlog
        CURL::libcurl
        nlohmann_json::nlohmann_json
        PkgConfig::AVCODEC
        PkgConfig::AVFORMAT
        PkgConfig::AVUTIL
        PkgConfig::SWSCALE
)

set_target_properties(YomitanMPV PROPERTIES
    PREFIX ""
)

if (COPY_BUILD_TO_MPV_SCRIPTS)
    add_custom_command(TARGET YomitanMPV POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:YomitanMPV>
            "~/.config/mpv/scripts"
    )
endif ()