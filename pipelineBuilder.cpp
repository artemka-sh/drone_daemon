//
// Created by arm64 on 21.08.2026.
//

#include "pipelineBuilder.h"
#include "cameraCapture.h"
#include "analyzer.h"
#include <print>
#include <chrono>

PipelineBuilder::PipelineBuilder() = default;

PipelineBuilder::~PipelineBuilder()
{
    cleanup();
}

void PipelineBuilder::run()
{

    std::vector<std::string> pipelines = {
        // Пайплайн 0: Отправка в сеть + Аппаратное декодирование для AI
        "v4l2src device=/dev/video0 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse config-interval=1 ! "
        "tee name=t "
        "t. ! queue ! rtspclientsink location=rtsp://127.0.0.1:8554/cam0 protocols=tcp "
        "t. ! queue ! vah264dec ! video/x-raw,format=NV12 ! appsink drop=true max-buffers=1 sync=false",

        // Пайплайн 1 (для второй камеры /dev/video2)
        "v4l2src device=/dev/video2 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse config-interval=1 ! "
        "tee name=t "
        "t. ! queue ! rtspclientsink location=rtsp://127.0.0.1:8554/cam2 protocols=tcp "
        "t. ! queue ! vah264dec ! video/x-raw,format=NV12 ! appsink drop=true max-buffers=1 sync=false",

        // Пайплайн 2 (для третьей камеры /dev/video4)
      "v4l2src device=/dev/video4 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse config-interval=1 ! "
      "tee name=t "
      "t. ! queue ! rtspclientsink location=rtsp://127.0.0.1:8554/cam4 protocols=tcp "
      "t. ! queue ! vah264dec ! video/x-raw,format=NV12 ! appsink drop=true max-buffers=1 sync=false",

        // Пайплайн 3 (для четвёртной камеры /dev/video6)
      "v4l2src device=/dev/video6 ! video/x-h264,width=1280,height=720,framerate=30/1 ! h264parse config-interval=1 ! "
      "tee name=t "
      "t. ! queue ! rtspclientsink location=rtsp://127.0.0.1:8554/cam6 protocols=tcp "
      "t. ! queue ! vah264dec ! video/x-raw,format=NV12 ! appsink drop=true max-buffers=1 sync=false"
    };

    for (size_t i = 0; i < pipelines.size(); ++i) {
        auto cam = std::make_unique<VideoProducer>(i, pipelines[i], frame_queue);
        cam->start();
        cameras.push_back(std::move(cam));
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

    std::println("Пайплайн запущен. Нажмите ctrl+c для остановки...");

}

void PipelineBuilder::cleanup()
{
    for (auto& cam : cameras) cam->stop();
    for (auto& ai : ai_workers) ai->stop();
}
