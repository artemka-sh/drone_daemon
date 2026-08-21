#include <iostream>
#include <functional>
#include <print>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>
#include <opencv5/opencv2/opencv.hpp>

int main()
{
    // Открываем камеры через GStreamer по точным путям из вашего v4l2-ctl
    std::vector<cv::VideoCapture> caps{
        cv::VideoCapture("v4l2src device=/dev/video2 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse ! avdec_h264 ! videoconvert ! appsink drop=true max-buffers=1", cv::CAP_GSTREAMER),
        cv::VideoCapture("v4l2src device=/dev/video4 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse ! avdec_h264 ! videoconvert ! appsink drop=true max-buffers=1", cv::CAP_GSTREAMER),
        cv::VideoCapture("v4l2src device=/dev/video6 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse ! avdec_h264 ! videoconvert ! appsink drop=true max-buffers=1", cv::CAP_GSTREAMER),
        cv::VideoCapture("v4l2src device=/dev/video0 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse ! avdec_h264 ! videoconvert ! appsink drop=true max-buffers=1", cv::CAP_GSTREAMER)
    };

    if (std::ranges::any_of(caps, std::not_fn(&cv::VideoCapture::isOpened))) {
        std::println(std::cerr, "Ошибка: Не удалось открыть одну или несколько камер через GStreamer!");
        return -1;
    }

    for (size_t i = 0; i < caps.size(); ++i) {
        cv::namedWindow("Camera " + std::to_string(i), cv::WINDOW_NORMAL);
    }

    std::vector<cv::Mat> frames(caps.size());
    std::vector<std::mutex> mutexes(caps.size());
    std::atomic<bool> running{true};
    std::vector<std::jthread> workers;

    // Запускаем независимый поток чтения для каждой камеры
    for (size_t i = 0; i < caps.size(); ++i) {
        workers.emplace_back([i, &caps, &frames, &mutexes, &running]() {
            while (running) {
                cv::Mat tmp;
                caps[i] >> tmp;

                if (!tmp.empty()) {
                    std::lock_guard<std::mutex> lock(mutexes[i]);
                    frames[i] = tmp;
                }
            }
        });
    }

    std::println("Потоки успешно запущены на H.264. Для выхода нажмите 'Esc'.");

    // Главный цикл отрисовки интерфейса
    for (;;) {
        for (size_t i = 0; i < caps.size(); ++i) {
            cv::Mat current_frame;
            {
                std::lock_guard<std::mutex> lock(mutexes[i]);
                if (!frames[i].empty()) {
                    std::swap(current_frame, frames[i]);
                }
            }

            if (!current_frame.empty()) {
                cv::imshow("Camera " + std::to_string(i), current_frame);
            }
        }

        char key = (char)cv::waitKey(1);
        if (key == 27) {
            break;
        }
    }

    // Завершаем работу фоновых потоков
    running = false;
    workers.clear();

    std::ranges::for_each(caps, &cv::VideoCapture::release);
    cv::destroyAllWindows();

    return 0;
}
