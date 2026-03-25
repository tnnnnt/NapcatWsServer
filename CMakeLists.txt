cmake_minimum_required(VERSION 3.16)
project(NapcatWsServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package(Boost REQUIRED COMPONENTS system)

# JSON
find_package(nlohmann_json REQUIRED)

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    core/bot_client.cpp
    net/websocket_server.cpp
    logic/command_router.cpp
    logic/message/handle_message.cpp
    logic/notice/handle_notice.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::system
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# 编译警告（推荐）
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra
)