cmake_minimum_required(VERSION 3.28)
project(csplatting C)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

set(CMAKE_C_STANDARD 11)

add_executable(csplatting
        csplatting.c)
