cmake_minimum_required(VERSION 3.13)
project(pop3)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(postboxes)
include_directories(postboxes/jack)
include_directories(postboxes/tom)
include_directories(minini_12b)

add_executable(pop3
        postboxes/tom/1
        postboxes/tom/2
        postboxes/tom/account.ini
        main.c
        pop3.c
        pop3.h
        minini_12b/minIni.h
        minini_12b/minIni.c
        postboxes.ini
        )
