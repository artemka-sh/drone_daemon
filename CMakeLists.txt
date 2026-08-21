cmake_minimum_required(VERSION 4.4)
project(drone_daemon)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 26)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})
