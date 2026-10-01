cmake_minimum_required(VERSION 3.15)
project(MyProject CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MMA_client main.cpp)
if(WIN32)
    target_link_libraries(MMA_client PRIVATE ws2_32)
    target_link_options(MMA_client PRIVATE
        -static-libgcc
        -static-libstdc++
        -static)
endif()
