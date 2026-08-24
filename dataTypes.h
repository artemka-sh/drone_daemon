//
// Created by arm64 on 21.08.2026.
//

#ifndef DRONE_DAEMON_DATATYPES_H
#define DRONE_DAEMON_DATATYPES_H
#include <opencv2/core/mat.hpp>


struct FrameTask {
    int camera_id;
    cv::Mat frame; // Здесь будет сырой NV12 или BGR
};

struct DetectionBox {
    int x, y, width, height;
    std::string object_class;
};

struct InferenceResult {
    int camera_id;
    std::vector<DetectionBox> detections;
};


#endif //DRONE_DAEMON_DATATYPES_H
