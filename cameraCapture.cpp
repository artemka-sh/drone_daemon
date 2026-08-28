#include "cameraCapture.h"
#include <gst/app/gstappsink.h>
#include <opencv2/core.hpp>
#include <iostream>

void VideoProducer::start()
{
    running_ = true;
    worker_ = std::jthread([this] { run(); });
}

void VideoProducer::stop()
{
    std::cout << "[Камера " << id_ << "] stop() вызван" << std::endl;
    running_ = false;
}

VideoProducer::~VideoProducer()
{
    stop();
    if (worker_.joinable()) worker_.join();
}

bool VideoProducer::buildPipeline()
{
    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_desc_.c_str(), &error);
    if (!pipeline_ || error) {
        std::cerr << "[Камера " << id_ << "] Ошибка сборки пайплайна: "
                  << (error ? error->message : "неизвестная ошибка") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "appsink0");
    if (!appsink_) {
        std::cerr << "[Камера " << id_ << "] В пайплайне не найден элемент с именем appsink0"
                  << std::endl;
        return false;
    }

    bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[Камера " << id_ << "] Не удалось перевести пайплайн в PLAYING"
                  << std::endl;
        return false;
    }

    return true;
}

void VideoProducer::teardownPipeline()
{
    if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_NULL);
    if (appsink_)  { gst_object_unref(appsink_);  appsink_  = nullptr; }
    if (bus_)      { gst_object_unref(bus_);      bus_      = nullptr; }
    if (pipeline_) { gst_object_unref(pipeline_); pipeline_ = nullptr; }
}

void VideoProducer::run()
{
    if (!buildPipeline()) {
        teardownPipeline();
        return;
    }

    std::cout << "[Камера " << id_ << "] Пайплайн запущен, ожидаем кадры..." << std::endl;

    while (running_) {
        // Неблокирующая проверка шины: ошибка (например, камера физически
        // отвалилась) или EOS — сразу выходим из цикла, отдельно не крашим процесс.
        if (GstMessage* msg = gst_bus_pop_filtered(
                bus_, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError* err = nullptr;
                gchar* dbg = nullptr;
                gst_message_parse_error(msg, &err, &dbg);
                std::cerr << "[Камера " << id_ << "] Ошибка GStreamer: "
                          << (err ? err->message : "?") << std::endl;
                if (err) g_error_free(err);
                if (dbg) g_free(dbg);
            } else {
                std::cout << "[Камера " << id_ << "] Получен EOS" << std::endl;
            }
            gst_message_unref(msg);
            break;
        }

        // Таймаут 200 мс — не блокируем поток навечно, чтобы успевать проверять running_/bus_.
        GstSample* sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(appsink_), 200 * GST_MSECOND);
        if (!sample) continue;

        GstCaps* caps = gst_sample_get_caps(sample);
        GstStructure* s = gst_caps_get_structure(caps, 0);
        int width = 0, height = 0;
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            // caps пайплайна фиксируют video/x-raw,format=BGR — раскладка байт
            // совпадает с тем, что ждёт cv::Mat, поэтому оборачиваем без конвертации.
            cv::Mat frame(height, width, CV_8UC3, map.data);
            out_queue_.push(FrameTask{id_, frame.clone()}); // clone — данные буфера живут только до unmap
            gst_buffer_unmap(buffer, &map);
        }

        gst_sample_unref(sample);
    }

    teardownPipeline();
    std::cout << "[Камера " << id_ << "] Поток остановлен" << std::endl;
}