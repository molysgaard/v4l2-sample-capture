cmake_minimum_required(VERSION 2.8.12)
project(v4l2-sample-capture)

ADD_EXECUTABLE(v4l2-sample-capture main.cpp)

install(TARGETS v4l2-sample-capture
    RUNTIME DESTINATION bin
)
