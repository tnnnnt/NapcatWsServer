cmake_minimum_required(VERSION 3.16)
project(NapcatWsServer)

set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Boost REQUIRED COMPONENTS system json)
find_package(OpenSSL REQUIRED)

add_executable(NapcatWsServer main.cpp)

target_link_libraries(NapcatWsServer PRIVATE Boost::system Boost::json OpenSSL::SSL OpenSSL::Crypto)
