cmake_minimum_required(VERSION 3.16)  

project(OrderBook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(OrderBook
    src/main.cpp
)

target_include_directories(OrderBook PRIVATE include)

add_executable(MarketReplay replay/replay.cpp)
target_include_directories(MarketReplay PRIVATE include)

add_executable(ReferenceReplay replay/reference.cpp)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME execution_simulation
        COMMAND ${CMAKE_COMMAND}
            -DREPLAY=$<TARGET_FILE:MarketReplay>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/replay/test_fills.cmake)
endif()
