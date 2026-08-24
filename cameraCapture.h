//
// Created by arm64 on 21.08.2026.
//

#ifndef DRONE_DAEMON_CAMERACAPTURE_H
#define DRONE_DAEMON_CAMERACAPTURE_H
#include <string>
#include <thread>

#include "threadSafeQueue.h"
#include "dataTypes.h"


class VideoProducer {
public:
    VideoProducer(int id, std::string pipeline, ThreadSafeQueue<FrameTask>& out_queue)
        : id_(id), pipeline_(std::move(pipeline)), out_queue_(out_queue) {}

    void start();

    void stop();

private:
    void run();

    int id_;
    std::string pipeline_;
    ThreadSafeQueue<FrameTask>& out_queue_;
    std::atomic<bool> running_{false};
    std::jthread worker_;
};




#endif //DRONE_DAEMON_CAMERACAPTURE_H
