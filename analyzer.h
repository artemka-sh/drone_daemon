//
// Created by arm64 on 21.08.2026.
//

#ifndef DRONE_DAEMON_ANALYZER_H
#define DRONE_DAEMON_ANALYZER_H
#include <thread>
#include "dataTypes.h"
#include "threadSafeQueue.h"


class AIProcessor {
public:
    AIProcessor(int worker_id, ThreadSafeQueue<FrameTask>& in_queue, ThreadSafeQueue<InferenceResult>& out_queue);

    void start();

    void stop();

private:
    void run();

    int worker_id_;
    ThreadSafeQueue<FrameTask>& in_queue_;
    ThreadSafeQueue<InferenceResult>& out_queue_;
    std::atomic<bool> running_{false};
    std::jthread worker_;
};


#endif //DRONE_DAEMON_ANALYZER_H
