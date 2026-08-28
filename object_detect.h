#ifndef OBJECT_DETECT_H
#define OBJECT_DETECT_H

#include "acl/acl.h"
#include "model_process.h"
#include <opencv2/opencv.hpp>
#include "dataTypes.h"
#include "utils.h"
#include <memory>
#include <vector>

class ObjectDetect {
public:
    ObjectDetect(const char* modelPath, uint32_t modelWidth, uint32_t modelHeight);
    ~ObjectDetect();
    
    Result Init();
    Result Preprocess(cv::Mat& frame);
    Result Inference(aclmdlDataset*& inferenceOutput);
    Result Postprocess(cv::Mat& frame, aclmdlDataset* modelOutput, std::vector<DetectionResult>& out_bboxes);

private:
    Result InitResource();
    Result InitModel(const char* omModelPath);
    Result CreateModelInputdDataset();
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
    uint32_t imageInfoSize_;
    void* imageInfoBuf_;
    bool isInited_;
};

#endif