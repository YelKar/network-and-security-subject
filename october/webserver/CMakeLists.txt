set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(webserver
    src/main.cpp
    src/Config.cpp
    src/MimeTypes.cpp
    src/SignalWatcher.cpp
    src/HttpServer.cpp
)

target_include_directories(webserver PRIVATE include)
target_compile_definitions(webserver PRIVATE _GNU_SOURCE)
target_compile_options(webserver PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(webserver PRIVATE Threads::Threads)

option(WEBSERVER_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(WEBSERVER_BUILD_TESTS "Build GoogleTest integration tests" ON)

if(WEBSERVER_SANITIZE)
    target_compile_options(webserver PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(webserver PRIVATE -fsanitize=address,undefined)
endif()

if(WEBSERVER_BUILD_TESTS)
    find_package(GTest QUIET)
    find_package(OpenSSL QUIET COMPONENTS Crypto)
    if(GTest_FOUND AND OpenSSL_FOUND)
        add_executable(webserver_tests tests/test_server.cpp)
        target_include_directories(webserver_tests PRIVATE include)
        target_compile_definitions(webserver_tests PRIVATE _GNU_SOURCE)
        target_compile_options(webserver_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(webserver_tests PRIVATE GTest::gtest_main OpenSSL::Crypto Threads::Threads)
        target_compile_definitions(webserver_tests PRIVATE
            SERVER_BIN_PATH="$<TARGET_FILE:webserver>"
        )

        if(WEBSERVER_SANITIZE)
            target_compile_options(webserver_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(webserver_tests PRIVATE -fsanitize=address,undefined)
        endif()

        enable_testing()
        add_test(
            NAME webserver_integration
            COMMAND ${CMAKE_COMMAND} -E env
                WEBSERVER_BINARY=$<TARGET_FILE:webserver>
                WEBSERVER_PUBLIC=${CMAKE_CURRENT_SOURCE_DIR}/public
                $<TARGET_FILE:webserver_tests>
        )
        set_tests_properties(webserver_integration PROPERTIES TIMEOUT 120)
    else()
        message(WARNING "GoogleTest or OpenSSL not found; integration tests are disabled")
    endif()
endif()
