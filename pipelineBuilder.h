//
// Created by arm64 on 21.08.2026.
//

#ifndef DRONE_DAEMON_PIPELINEBUILDER_H
#define DRONE_DAEMON_PIPELINEBUILDER_H

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "threadSafeQueue.h"
#include "dataTypes.h"
#include "frontendServer.h"

class VideoProducer;
class AIProcessor;


class PipelineBuilder
{
    uint m_threads_of_workers = 2;
    std::atomic<bool> server_running{true};
    ThreadSafeQueue<FrameTask> frame_queue{10};
    ThreadSafeQueue<InferenceResult> result_queue{100};// Метаданные легкие, лимит больше
    std::vector<std::unique_ptr<VideoProducer>> cameras;
    std::vector<std::unique_ptr<AIProcessor>> ai_workers;
    FrontendServer frontend_server;
    std::jthread network_server;

public:
    PipelineBuilder();
    ~PipelineBuilder();
    void run();
    void cleanup();
};


#endif //DRONE_DAEMON_PIPELINEBUILDER_H
