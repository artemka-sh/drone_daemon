#include "cameraCapture.h"
#include <gst/app/gstappsink.h>
#include <opencv2/core.hpp>
#include <thread>
#include <chrono>

void VideoProducer::start()
{
    running_ = true;
    worker_ = std::jthread([this] { run(); });
}

void VideoProducer::stop()
{
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
        if (error) g_error_free(error);
        return false;
    }

    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "appsink0");
    if (!appsink_) return false;

    bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) return false;

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
    int no_frame_counter = 0;

    while (running_) {
        if (!pipeline_) {
            if (!buildPipeline()) {
                teardownPipeline();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            no_frame_counter = 0;
        }

        if (GstMessage* msg = gst_bus_pop_filtered(
                bus_, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
            gst_message_unref(msg);
            teardownPipeline();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
                }

        GstSample* sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(appsink_), 200 * GST_MSECOND);

        if (!sample) {
            no_frame_counter++;
            if (no_frame_counter > 15) {
                std::cerr << "[Камера " << id_ << "] Тайм-аут (кадров нет). Принудительный рестарт..." << std::endl;
                teardownPipeline();
                no_frame_counter = 0;
            }
            continue;
        }

        no_frame_counter = 0;

        GstCaps* caps = gst_sample_get_caps(sample);
        GstStructure* s = gst_caps_get_structure(caps, 0);
        int width = 0, height = 0;
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            cv::Mat frame(height, width, CV_8UC3, map.data);
            out_queue_.push(FrameTask{id_, frame.clone()});
            gst_buffer_unmap(buffer, &map);
        }

        gst_sample_unref(sample);
    }

    teardownPipeline();
}