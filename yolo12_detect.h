#ifndef YOLO12_DETECT_H
#define YOLO12_DETECT_H

#include "acl/acl.h"
#include "model_process.h"
#include <opencv2/opencv.hpp>
#include "dataTypes.h"
#include "utils.h"
#include <vector>

// Детектор для YOLO12 (anchor-free, единый выходной тензор [1, 4+nc, N]).
// В отличие от ObjectDetect (заточен под YOLOv3-сэмпл с готовым NMS внутри
// .om), тут decode боксов и NMS делаются на CPU в Postprocess(), т.к.
// стандартная ONNX-экспортированная YOLO12/YOLOv8-архитектура отдаёт
// сырые предсказания без NMS.
class Yolo12Detect {
public:
    Yolo12Detect(const char* modelPath, uint32_t modelWidth, uint32_t modelHeight,
                 float confThresh = 0.25f, float iouThresh = 0.45f);
    ~Yolo12Detect();

    Result Init();
    Result Preprocess(cv::Mat& frame);
    Result Inference(aclmdlDataset*& inferenceOutput);
    Result Postprocess(aclmdlDataset* modelOutput, std::vector<DetectionResult>& out_bboxes);

private:
    Result InitResource();
    Result InitModel(const char* omModelPath);
    Result CreateModelInputDataset();
    void* GetInferenceOutputItem(uint32_t& itemDataSize, aclmdlDataset* inferenceOutput, uint32_t idx);
    void DestroyResource();

    int32_t deviceId_;
    aclrtRunMode runMode_;
    ModelProcess model_;

    const char* modelPath_;
    uint32_t modelWidth_;
    uint32_t modelHeight_;
    uint32_t imageDataSize_;
    void* imageDataBuf_;
    bool isInited_;

    float confThresh_;
    float iouThresh_;

    // letterbox-параметры последнего обработанного кадра — нужны, чтобы
    // вернуть координаты боксов обратно в систему координат исходного кадра
    float scale_ = 1.f;
    int padLeft_ = 0;
    int padTop_ = 0;
    int origWidth_ = 0;
    int origHeight_ = 0;
};

#endif
