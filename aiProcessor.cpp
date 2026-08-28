//
// Created by arm64 on 28.08.2026.
//
#include "aiProcessor.h"
#include "yolo12_detect.h"
#include <iostream>

AIProcessor::AIProcessor(int id, ThreadSafeQueue<FrameTask>& in_queue, ThreadSafeQueue<ResultTask>& out_queue)
    : id_(id), in_queue_(in_queue), out_queue_(out_queue) {}

void AIProcessor::start() {
    running_ = true;
    worker_ = std::jthread([this] { run(); });
}

void AIProcessor::stop() {
    std::cout << "[ИИ Воркер " << id_ << "] stop() вызван" << std::endl;
    running_ = false;
}

AIProcessor::~AIProcessor() {
    stop();
    if (worker_.joinable()) worker_.join();
}

void AIProcessor::run() {
    // Путь и размер входа — под конвертированную yolo12n.om (640x640,
    // экспортирована с imgsz=640). Если экспортировал с другим imgsz —
    // поменяй тут и в --input_shape при конвертации atc.
    Yolo12Detect detect("../model/yolo12n.om", 640, 640);

    Result ret = detect.Init();
    if (ret != SUCCESS) {
        std::cerr << "[ИИ Воркер " << id_ << "] Ошибка инициализации модели!" << std::endl;
        return;
    }

    std::cout << "[ИИ Воркер " << id_ << "] Запущен и готов к работе" << std::endl;

    while (running_) {
        auto task_opt = in_queue_.pop_with_timeout(std::chrono::milliseconds(100));
        if (!task_opt) continue;

        FrameTask task = *task_opt;

        ret = detect.Preprocess(task.frame);
        if (ret != SUCCESS) {
            std::cerr << "[ИИ Воркер " << id_ << "] Ошибка Preprocess" << std::endl;
            continue;
        }

        aclmdlDataset* inferenceOutput = nullptr;
        ret = detect.Inference(inferenceOutput);
        if ((ret != SUCCESS) || (inferenceOutput == nullptr)) {
            std::cerr << "[ИИ Воркер " << id_ << "] Ошибка Inference" << std::endl;
            continue;
        }

        std::vector<DetectionResult> bboxes;
        ret = detect.Postprocess(inferenceOutput, bboxes);
        if (ret != SUCCESS) {
            std::cerr << "[ИИ Воркер " << id_ << "] Ошибка Postprocess" << std::endl;
            continue;
        }

        out_queue_.push(ResultTask{task.cam_id, std::move(bboxes)});
    }

    std::cout << "[ИИ Воркер " << id_ << "] Остановлен" << std::endl;
}
