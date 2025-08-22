cmake_minimum_required(VERSION 3.20)
project(MarketDataEngine VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address,undefined")

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# JSON library
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(market_data_core
    src/core/market_data_types.cpp
    src/core/lock_free_queue.cpp
    src/core/memory_pool.cpp
    src/core/simd_operations.cpp
    src/protocols/fix_parser.cpp
    src/protocols/binary_parser.cpp
    src/feed_handler/feed_processor.cpp
    src/feed_handler/order_book.cpp
    src/analytics/market_metrics.cpp
    src/analytics/latency_tracker.cpp
)

target_link_libraries(market_data_core 
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Market data simulator
add_executable(market_simulator
    src/simulator/market_simulator.cpp
    src/simulator/data_generator.cpp
)

target_link_libraries(market_simulator 
    market_data_core
    Threads::Threads
)

# Simple WebSocket server (without WebSocket++ for now)
add_executable(websocket_server
    src/server/simple_server.cpp
)

target_link_libraries(websocket_server 
    market_data_core
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# WebSocket server with real WebSocket protocol
add_executable(ws_server
    src/server/ws_server.cpp
)

target_link_libraries(ws_server 
    market_data_core
    Threads::Threads
    nlohmann_json::nlohmann_json
    crypto
    ssl
)

# Performance benchmarks
add_executable(benchmark
    src/benchmark/latency_benchmark.cpp
    src/benchmark/throughput_benchmark.cpp
)

target_link_libraries(benchmark 
    market_data_core
    Threads::Threads
)

# Main feed handler application
add_executable(feed_handler
    src/main.cpp
)

target_link_libraries(feed_handler 
    market_data_core
    Threads::Threads
)

# Enable testing (commented out for now)
# enable_testing()
# add_subdirectory(tests)

# Install targets
install(TARGETS feed_handler market_simulator websocket_server ws_server benchmark
    RUNTIME DESTINATION bin
) 