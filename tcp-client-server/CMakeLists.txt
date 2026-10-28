set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_definitions(_POSIX_C_SOURCE=200809L)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
        include
)

add_executable(client
        src/client.c
        src/client_core.c
        src/message.c
        src/util.c
        src/config.c
)

add_executable(server
        src/sync/server.c
        src/message.c
        src/util.c
        src/config.c
)

add_executable(multiprocess-server
        src/multiprocess/server.c
        src/message.c
        src/util.c
        src/config.c
)

add_compile_definitions(_GNU_SOURCE)
add_executable(concurrent-server
        src/multithreading/server.c
        src/multithreading/thread_pool.c
        src/message.c
        src/util.c
        src/config.c
)
target_link_options(concurrent-server PRIVATE -pthread)
target_compile_options(concurrent-server PRIVATE -pthread)