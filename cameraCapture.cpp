//
// Created by arm64 on 21.08.2026.
//

#include "cameraCapture.h"
#include <opencv2/videoio.hpp>
#include <iostream>

void VideoProducer::start()
{
    running_ = true;
    worker_ = std::jthread([this] { run(); });
}

void VideoProducer::stop()
{
    running_ = false;
}

void VideoProducer::run()
{
    cv::VideoCapture cap(pipeline_, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cout << "Ошибка: Камера {} не открылась" << id_ << std::endl;
        return;
    }

    cv::Mat frame;
    while (running_) {
        cap >> frame;
        if (!frame.empty()) {
            // Перемещаем кадр в очередь
            out_queue_.push(FrameTask{id_, std::move(frame)});
        }
    }
    cap.release();
}
