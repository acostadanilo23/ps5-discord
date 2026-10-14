cmake_minimum_required(VERSION 3.20)
project(ps5_discord LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PS5_DISCORD_BUILD_TESTS "Build host tests" ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED IMPORTED_TARGET opus)

add_library(ps5discord_core
    src/config.cpp
    src/discord/rest_client.cpp
    src/discord/service.cpp
    src/voice/protocol.cpp
    src/audio/opus.cpp
)

target_include_directories(ps5discord_core PUBLIC include)
target_link_libraries(ps5discord_core PUBLIC CURL::libcurl Threads::Threads PkgConfig::OPUS)
target_compile_definitions(ps5discord_core PUBLIC CURL_STATICLIB)

if(MSVC)
    target_compile_options(ps5discord_core PRIVATE /W4)
else()
    target_compile_options(ps5discord_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ps5-discord-host apps/host/main.cpp)
target_link_libraries(ps5-discord-host PRIVATE ps5discord_core)

if(PS5_DISCORD_BUILD_TESTS)
    enable_testing()
    add_executable(ps5-discord-tests tests/core_tests.cpp)
    target_link_libraries(ps5-discord-tests PRIVATE ps5discord_core)
    add_test(NAME ps5-discord-tests COMMAND ps5-discord-tests)
endif()
