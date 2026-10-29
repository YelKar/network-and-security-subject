set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

add_executable(dns
        src/main.cpp
        src/Socket.cpp
        src/BinaryBuffer.cpp
        src/DNSPackage.cpp
        src/Resolver.cpp
        src/DebugStream.cpp
)