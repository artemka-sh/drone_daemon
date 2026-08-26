//
// Created by arm64 on 21.08.2026.
//

#include "analyzer.h"
#include <iostream>

AIProcessor::AIProcessor(int worker_id, ThreadSafeQueue<FrameTask>& in_queue,
    ThreadSafeQueue<InferenceResult>& out_queue): worker_id_(worker_id), in_queue_(in_queue), out_queue_(out_queue)
{}

void AIProcessor::start()
{
    running_ = true;
    worker_ = std::jthread([this] { run(); });
}

void AIProcessor::stop()
{
    running_ = false;
}

void AIProcessor::run()
{
    // Здесь можно загрузить модель YOLO (AscendCL, NCNN, ONNX и т.д.)
    std::cout << "AI Processor {} запущен: " << worker_id_ << std::endl;

    while (running_) {
        // Ждем кадр максимум 100 мс, затем проверяем флаг running_
        auto task_opt = in_queue_.pop_with_timeout(std::chrono::milliseconds(100));

        if (task_opt) {
            FrameTask& task = *task_opt;

            // --- СИМУЛЯЦИЯ ТЯЖЕЛОЙ РАБОТЫ ---
            // cv::Mat gray = task.frame(cv::Rect(0, 0, 1280, 720)); // если это NV12
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // эмуляция инференса

            // Формируем фейковый результат
            InferenceResult res;
            res.camera_id = task.camera_id;
            res.detections.push_back({10, 10, 100, 100, "defect"});

            out_queue_.push(std::move(res));
        }
    }
}