cmake_minimum_required(VERSION 4.0)
project(Ignis)

set(CMAKE_CXX_STANDARD 17)

add_executable(Ignis main.cpp
        frontend/lexer/lexer.h
        frontend/token/token.h)
