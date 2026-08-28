cmake_minimum_required(VERSION 3.0...4.4)
project(drone_daemon)

set(CMAKE_CXX_STANDARD 20)

# --- ЖЕСТКИЕ ПУТИ ДЛЯ ORANGE PI AIPRO (CANN) ---
# CLion по SSH может не подтягивать ENV, поэтому прописываем пути явно
set(CANN_BASE "/usr/local/Ascend/ascend-toolkit/latest")

link_directories(
        ${CANN_BASE}/lib64
        ${CANN_BASE}/acllib/lib64
        ${CANN_BASE}/aarch64-linux/lib64
        /usr/local/Ascend/driver/lib64
)
# --------------------------------------------------

# Подключаем инструмент PkgConfig
find_package(PkgConfig REQUIRED)

# Ищем GStreamer
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)

find_package(OpenCV REQUIRED)

set(HTTPLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib/httplib.h")
if(NOT EXISTS "${HTTPLIB_PATH}")
    file(DOWNLOAD "https://raw.githubusercontent.com/yhirose/cpp-httplib/refs/heads/master/httplib.h" "${HTTPLIB_PATH}")
endif()

add_executable(${PROJECT_NAME}
        main.cpp
        cameraCapture.cpp
        pipelineBuilder.cpp
        aiProcessor.cpp
        yolo12_detect.cpp
        model_process.cpp
        utils.cpp
)


target_include_directories(${PROJECT_NAME} PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/deps/
        ${GST_INCLUDE_DIRS}
        # Точные пути к заголовкам acl.h на Orange Pi:
        ${CANN_BASE}/include
        ${CANN_BASE}/acllib/include
        ${CANN_BASE}/aarch64-linux/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${OpenCV_LIBS}
        ${GST_LIBRARIES}
        ascendcl
        stdc++
)

execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/res ${CMAKE_BINARY_DIR}/res
)