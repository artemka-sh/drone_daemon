cmake_minimum_required(VERSION 4.4)
project(drone_daemon)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 26)

set(HTTPLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib/httplib.h")
if(NOT EXISTS "${HTTPLIB_PATH}")
    file(DOWNLOAD "https://raw.githubusercontent.com/yhirose/cpp-httplib/refs/heads/master/httplib.h" "${HTTPLIB_PATH}")
endif()

add_executable(${PROJECT_NAME} main.cpp
        threadSafeQueue.h
        dataTypes.h
        analyzer.cpp
        analyzer.h
        cameraCapture.cpp
        cameraCapture.h
        pipelineBuilder.cpp
        pipelineBuilder.h
        frontendServer.h
        deps/cpp-httplib/httplib.h)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/deps/)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})
execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/res ${CMAKE_BINARY_DIR}/res
)