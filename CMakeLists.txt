# 设置最低的 CMake 版本
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
project(thread-pool)
set(HOME ${CMAKE_CURRENT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH ${HOME}/bin)


include_directories(${HOME}/include)

link_directories(${HOME}/bin)
aux_source_directory(${HOME}/src SRC_PATH)

add_executable(app ${SRC_PATH})
