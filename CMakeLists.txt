cmake_minimum_required(VERSION 3.5)
project(wsdemo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
link_libraries (websockets czmq)
set(SOURCE_FILES main.cpp jsmn.c jsmn.h)
add_executable(wsdemo ${SOURCE_FILES})