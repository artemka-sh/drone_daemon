//
// Created by arm64 on 21.08.2026.
//

#include "pipelineBuilder.h"
#include "cameraCapture.h"
#include "analyzer.h"
#include <print>
#include <chrono>
#include <filesystem>

PipelineBuilder::PipelineBuilder() = default;

PipelineBuilder::~PipelineBuilder()
{
    cleanup();
}

void PipelineBuilder::run()
{

    std::vector<int> available_devices;
    for (int dev_id : {0, 2, 4, 6}) {
        if (std::filesystem::exists("/dev/video" + std::to_string(dev_id))) {
            available_devices.push_back(dev_id);
        } else {
            std::cout << "[INFO] Камера /dev/video" << dev_id << " не найдена. Пропускаем.\n";
        }
    }

    if (available_devices.empty()) {
        std::cout << "Не найдено ни одной камеры! Система работает вхолостую.\n";
    }

    auto build_pipeline = [](int dev_id) {
        std::string device = "/dev/video" + std::to_string(dev_id);
        std::string rtsp   = "rtsp://127.0.0.1:8554/cam" + std::to_string(dev_id);

        return "v4l2src device=" + device + " ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse config-interval=1 ! "
               "tee name=t "
               "t. ! queue ! rtspclientsink location=" + rtsp + " protocols=tcp "
               "t. ! queue ! vah264dec ! video/x-raw,format=NV12 ! appsink drop=true max-buffers=1 sync=false";
    };

    int internal_cam_id = 0;
    for (int dev_id : available_devices) {
        std::cout << "[START] Подключение камеры /dev/video" << dev_id << " (ID: " << internal_cam_id << ")...\n";

        std::string pipeline = build_pipeline(dev_id);

        auto cam = std::make_unique<VideoProducer>(internal_cam_id, pipeline, frame_queue);
        cam->start();
        cameras.push_back(std::move(cam));

        internal_cam_id++;
    }

    //надо столько воркеров сколько тянет параллельно обработчик
    for (int i = 0; i < m_threads_of_workers; ++i) {
        auto ai = std::make_unique<AIProcessor>(i, frame_queue, result_queue);
        ai->start();
        ai_workers.push_back(std::move(ai));
    }


    server_running = true;
    network_server = std::jthread([this]() {
        while (server_running) {
            auto res_opt = result_queue.pop_with_timeout(std::chrono::milliseconds(100));
            if (res_opt) {
                // Здесь будет отправка JSON через сеть
                // std::println("Сеть: Отправка метаданных для камеры {}", res_opt->camera_id);
            }
        }
    });

    frontend_server.start();

    std::cout << "Пайплайн запущен. Нажмите ctrl+c для остановки..." << std::endl;

}

void PipelineBuilder::cleanup()
{
    for (auto& cam : cameras) cam->stop();
    for (auto& ai : ai_workers) ai->stop();
}
