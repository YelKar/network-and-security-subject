set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

include_directories(include)

add_executable(smtp-client
        src/main.cpp
        src/SMTPClient.cpp
        src/core.cpp
)
target_link_libraries(smtp-client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
