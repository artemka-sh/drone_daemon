//
// Created by arm64 on 21.08.2026.
//
#include "pipelineBuilder.h"
#include "cameraCapture.h"
#include "aiProcessor.h"
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

    // ЭТАП 1: только захват + доставка сырого H264 в MediaMTX по RTP/UDP.
    // rtspclientsink на плате не найден (нет gst-rtsp-server), поэтому вместо
    // RTSP RECORD используем чистый RTP-пуш — он требует только rtph264pay/udpsink,
    // которые есть почти в любой сборке GStreamer (в отличие от отдельного
    // модуля gst-rtsp-server).
    //
    // ЭТАП 2 (текущий): appsink0 теперь ещё и наполняет frame_queue, откуда
    // кадры разбирают AI-воркеры (см. ниже). Разрешение appsink0 (640x480)
    // ниже, чем вход модели (640x640) — letterbox в Yolo12Detect::Preprocess
    // это компенсирует, но для лучшей точности разрешение стоит поднять
    // (см. закомментированную ai-ветку ниже как ориентир).
    auto build_pipeline = [](int dev_id) {
        std::string device = "/dev/video" + std::to_string(dev_id);
        int rtp_port = 5000 + dev_id;

        // ЭТАП 1: программный декодер (avdec_h264) — временная замена аппаратного
        // DVPP-декодирования на Ascend, только чтобы получить video/x-raw для appsink
        // прямо сейчас. На плате эту ветку заменит AscendVdecChannel (AscendCL VDEC),
        // появление кадра в out_queue_ на уровне VideoProducer при этом не изменится.
        return "v4l2src device=" + device + " ! "
               "video/x-h264,width=640,height=480,framerate=30/1 ! "
               "h264parse config-interval=1 ! "
               "tee name=t "
               "t. ! queue ! rtph264pay pt=96 config-interval=1 ! udpsink host=127.0.0.1 port=" +
                   std::to_string(rtp_port) + " sync=false "
               "t. ! queue ! avdec_h264 ! videoconvert ! video/x-raw,format=BGR ! "
                   "appsink name=appsink0 drop=true max-buffers=1 sync=false";
    };
        // ---- Ветка для ИИ (вернуть на плате вместе с AscendCL VDEC) ----
        // "v4l2src device=" + device + " ! video/x-h264,width=1280,height=720,framerate=30/1 ! "
        // "h264parse config-interval=1 ! tee name=t "
        // "t. ! queue ! rtph264pay pt=96 config-interval=1 ! udpsink host=127.0.0.1 port=" +
        //     std::to_string(rtp_port) + " sync=false "
        // "t. ! queue ! appsink name=ai_raw_" + std::to_string(dev_id) +
        //     " drop=true max-buffers=1 sync=false";

    int internal_cam_id = 0;
    for (int dev_id : available_devices) {
        std::cout << "[START] Подключение камеры /dev/video" << dev_id << " (ID: " << internal_cam_id << ")...\n";
        std::string pipeline = build_pipeline(dev_id);
        auto cam = std::make_unique<VideoProducer>(internal_cam_id, pipeline, frame_queue);
        cam->start();
        cameras.push_back(std::move(cam));
        internal_cam_id++;
    }

    // ЭТАП 2: AI-воркеры разбирают frame_queue и кладут результаты в result_queue.
    for (uint i = 0; i < m_threads_of_workers; ++i) {
        auto ai = std::make_unique<AIProcessor>(i, frame_queue, result_queue);
        ai->start();
        ai_workers.push_back(std::move(ai));
    }

    server_running = true;
    network_server = std::jthread([this]() {
        while (server_running) {
            auto res_opt = result_queue.pop_with_timeout(std::chrono::milliseconds(100));
            if (res_opt) {
                // TODO: сюда отдать ResultTask на фронт (JSON по WS/SSE) —
                // отдельная задача, скажи, если нужно её тоже сделать.
            }
        }
    });

    frontend_server.start();
    std::cout << "Пайплайн запущен (RTP/UDP -> MediaMTX, ИИ включён). Ctrl+C для остановки..." << std::endl;
}

void PipelineBuilder::cleanup()
{
    for (auto& cam : cameras) cam->stop();
    for (auto& ai : ai_workers) ai->stop();
}
