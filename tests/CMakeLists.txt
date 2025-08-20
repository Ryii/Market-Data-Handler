# Test configuration for Market Data Engine
cmake_minimum_required(VERSION 3.20)

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Fetch Google Test if not found
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Test executables
add_executable(test_queue
    test_lock_free_queue.cpp
)

add_executable(test_order_book
    test_order_book.cpp
)

add_executable(test_fix_parser
    test_fix_parser.cpp
)

add_executable(test_market_data_types
    test_market_data_types.cpp
)

add_executable(test_integration
    test_integration.cpp
)

# Link test executables
foreach(test_target test_queue test_order_book test_fix_parser test_market_data_types test_integration)
    target_link_libraries(${test_target}
        market_data_core
        gtest
        gtest_main
        Threads::Threads
    )
    
    # Add to CTest
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

# Performance tests (separate from unit tests)
add_executable(performance_test
    performance_test.cpp
)

target_link_libraries(performance_test
    market_data_core
    gtest
    gtest_main
    Threads::Threads
)

# Set test properties
set_target_properties(performance_test PROPERTIES
    TIMEOUT 60
)

add_test(NAME performance_test COMMAND performance_test) 