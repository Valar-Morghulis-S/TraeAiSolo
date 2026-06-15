cmake_minimum_required(VERSION 3.10)
project(demod_multi_thread_v2 VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/thread_pool.cpp
    src/demod_framework.cpp
    src/demod_api_mock.cpp
    src/main.cpp
)

add_executable(demod_demo ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(demod_demo Threads::Threads)

install(TARGETS demod_demo RUNTIME DESTINATION bin)