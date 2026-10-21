cmake_minimum_required(VERSION 3.20)

project(tcp-client-server LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_definitions(_POSIX_C_SOURCE=200809L)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(client
        client.c
        message.c
        util.c
        config.c
)

add_executable(server
        server.c
        message.c
        util.c
        config.c
)
