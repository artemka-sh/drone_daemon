#ifndef DRONE_DAEMON_AIPROCESSOR_H
#define DRONE_DAEMON_AIPROCESSOR_H

#include <atomic>
#include <thread>
#include "threadSafeQueue.h"
#include "dataTypes.h"
#include "object_detect.h"

class AIProcessor {
public:
    AIProcessor(int id, ThreadSafeQueue<FrameTask>& in_queue, ThreadSafeQueue<ResultTask>& out_queue);
    ~AIProcessor();

    void start();
    void stop();

private:
    void run();

    int id_;
    ThreadSafeQueue<FrameTask>& in_queue_;
    ThreadSafeQueue<ResultTask>& out_queue_;
    std::atomic<bool> running_{false};
    std::jthread worker_;
};

#endif // DRONE_DAEMON_AIPROCESSOR_H