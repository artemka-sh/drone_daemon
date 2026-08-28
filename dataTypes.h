#ifndef DRONE_DAEMON_DATATYPES_H
#define DRONE_DAEMON_DATATYPES_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <cstdint>

struct FrameTask {
    int cam_id;
    cv::Mat frame;
};

struct Point {
    uint32_t x;
    uint32_t y;
};

struct DetectionResult {
    Point lt;
    Point rb;
    std::string result_text;
};

struct ResultTask {
    int cam_id;
    std::vector<DetectionResult> bboxes;
};

#endif // DRONE_DAEMON_DATATYPES_H