#ifndef DRONE_DAEMON_CAMERACAPTURE_H
#define DRONE_DAEMON_CAMERACAPTURE_H

#include <string>
#include <atomic>
#include <thread>
#include <gst/gst.h>
#include "threadSafeQueue.h"
#include "dataTypes.h"

class VideoProducer {
public:
    VideoProducer(int id, std::string pipeline, ThreadSafeQueue<FrameTask>& out_queue)
        : id_(id), pipeline_desc_(std::move(pipeline)), out_queue_(out_queue) {}
    ~VideoProducer();

    void start();
    void stop();

private:
    void run();
    bool buildPipeline();
    void teardownPipeline();

    int id_;
    std::string pipeline_desc_;
    ThreadSafeQueue<FrameTask>& out_queue_;
    std::atomic<bool> running_{false};
    std::jthread worker_;

    GstElement* pipeline_ = nullptr;
    GstElement* appsink_  = nullptr;
    GstBus*     bus_      = nullptr;
};

#endif //DRONE_DAEMON_CAMERACAPTURE_H