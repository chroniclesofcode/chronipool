cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
project(chronipool)

add_compile_options(
  -Wall
  -Werror
)

add_subdirectory(include)

add_executable(main main.cpp)

target_include_directories(main
    PUBLIC
    ./include
)