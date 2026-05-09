cmake_minimum_required(VERSION 3.16)
project(kings_cards_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(kings_cards_c main.c)
add_executable(kings_cards_c_memman main_memman.c)
add_executable(c_artillery c_artillery.c)
add_executable(ptr_math ptr_math.c)
