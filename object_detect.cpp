#include "object_detect.h"
#include <iostream>

using namespace std;

namespace {
const static std::vector<std::string> yolov3Label = { "Солдат", "Танк", "Грузовик", "гражданский автомобиль",
    "Артилерия", "Самолёт", "Военный корабль" };

const uint32_t kBBoxDataBufId = 0;
const uint32_t kBoxNumDataBufId = 1;
enum BBoxIndex { TOPLEFTX = 0, TOPLEFTY, BOTTOMRIGHTX, BOTTOMRIGHTY, SCORE, LABEL };
}

ObjectDetect::ObjectDetect(const char* modelPath, uint32_t modelWidth, uint32_t modelHeight)
:deviceId_(0), imageDataBuf_(nullptr), imageInfoBuf_(nullptr), modelWidth_(modelWidth),
modelHeight_(modelHeight), isInited_(false){
    modelPath_ = modelPath;
    imageDataSize_ = RGBU8_IMAGE_SIZE(modelWidth, modelHeight);
}

ObjectDetect::~ObjectDetect() {
    DestroyResource();
}

Result ObjectDetect::InitResource() {
    const char *aclConfigPath = "../src/acl.json"; 
    aclError ret = aclInit(aclConfigPath);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("Acl init failed");
        return FAILED;
    }
    ret = aclrtSetDevice(deviceId_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("Acl open device %d failed", deviceId_);
        return FAILED;
    }
    ret = aclrtGetRunMode(&runMode_);
    if (ret != ACL_ERROR_NONE) return FAILED;
    return SUCCESS;
}

Result ObjectDetect::InitModel(const char* omModelPath) {
    Result ret = model_.LoadModelFromFileWithMem(omModelPath);
    if (ret != SUCCESS) return FAILED;
    ret = model_.CreateDesc();
    if (ret != SUCCESS) return FAILED;
    ret = model_.CreateOutput();
    if (ret != SUCCESS) return FAILED;
    return SUCCESS;
}

Result ObjectDetect::CreateModelInputdDataset() {
    aclError aclRet = aclrtMalloc(&imageDataBuf_, imageDataSize_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (aclRet != ACL_ERROR_NONE) return FAILED;
    
    const float imageInfo[4] = {(float)modelWidth_, (float)modelHeight_, (float)modelWidth_, (float)modelHeight_};
    imageInfoSize_ = sizeof(imageInfo);
    
    if (runMode_ == ACL_HOST)
        imageInfoBuf_ = Utils::CopyDataHostToDevice((void *)imageInfo, imageInfoSize_);
    else
        imageInfoBuf_ = Utils::CopyDataDeviceToDevice((void *)imageInfo, imageInfoSize_);
        
    if (imageInfoBuf_ == nullptr) return FAILED;
    
    Result ret = model_.CreateInput(imageDataBuf_, imageDataSize_, imageInfoBuf_, imageInfoSize_);
    if (ret != SUCCESS) return FAILED;
    return SUCCESS;
}

Result ObjectDetect::Init() {
    if (isInited_) return SUCCESS;
    if (InitResource() != SUCCESS) return FAILED;
    if (InitModel(modelPath_) != SUCCESS) return FAILED;
    if (CreateModelInputdDataset() != SUCCESS) return FAILED;
    isInited_ = true;
    return SUCCESS;
}

Result ObjectDetect::Preprocess(cv::Mat& frame) {
    cv::Mat reiszeMat;
    cv::resize(frame, reiszeMat, cv::Size(modelWidth_, modelHeight_));
    if (reiszeMat.empty()) return FAILED;
    
    aclrtMemcpyKind policy = (runMode_ == ACL_HOST) ? ACL_MEMCPY_HOST_TO_DEVICE : ACL_MEMCPY_DEVICE_TO_DEVICE;
    aclError ret = aclrtMemcpy(imageDataBuf_, imageDataSize_, reiszeMat.ptr<uint8_t>(), imageDataSize_, policy);
    if (ret != ACL_ERROR_NONE) return FAILED;
    
    return SUCCESS;
}

Result ObjectDetect::Inference(aclmdlDataset*& inferenceOutput) {
    Result ret = model_.Execute();
    if (ret != SUCCESS) return FAILED;
    inferenceOutput = model_.GetModelOutputData();
    return SUCCESS;
}

Result ObjectDetect::Postprocess(cv::Mat& frame, aclmdlDataset* modelOutput, std::vector<DetectionResult>& out_bboxes) {
    uint32_t dataSize = 0;
    float* detectData = (float*)GetInferenceOutputItem(dataSize, modelOutput, kBBoxDataBufId);
    if (detectData == nullptr) return FAILED;
    
    uint32_t* boxNum = (uint32_t*)GetInferenceOutputItem(dataSize, modelOutput, kBoxNumDataBufId);
    if (boxNum == nullptr) return FAILED;

    uint32_t totalBox = boxNum[0];
    float widthScale = (float)(frame.cols) / modelWidth_;
    float heightScale = (float)(frame.rows) / modelHeight_;

    std::vector<DetectionResult> detectResults;
    for (uint32_t i = 0; i < totalBox; i++) {
        uint32_t score = uint32_t(detectData[totalBox * SCORE + i] * 100);
        if (score < 80) continue;
        
        DetectionResult oneResult;
        oneResult.lt.x = detectData[totalBox * TOPLEFTX + i] * widthScale;
        oneResult.lt.y = detectData[totalBox * TOPLEFTY + i] * heightScale;
        oneResult.rb.x = detectData[totalBox * BOTTOMRIGHTX + i] * widthScale;
        oneResult.rb.y = detectData[totalBox * BOTTOMRIGHTY + i] * heightScale;
        
        uint32_t objIndex = (uint32_t)detectData[totalBox * LABEL + i];
        oneResult.result_text = yolov3Label[objIndex] + " " + std::to_string(score) + "%";
        
        detectResults.emplace_back(oneResult);
    }
    
    out_bboxes = detectResults;

    if (runMode_ == ACL_HOST) {
        delete[]((uint8_t*)detectData);
        delete[]((uint8_t*)boxNum);
    }
    return SUCCESS;
}

void* ObjectDetect::GetInferenceOutputItem(uint32_t& itemDataSize, aclmdlDataset* inferenceOutput, uint32_t idx) {
    aclDataBuffer* dataBuffer = aclmdlGetDatasetBuffer(inferenceOutput, idx);
    if (dataBuffer == nullptr) return nullptr;
    
    void* dataBufferDev = aclGetDataBufferAddr(dataBuffer);
    if (dataBufferDev == nullptr) return nullptr;
    
    size_t bufferSize = aclGetDataBufferSize(dataBuffer);
    if (bufferSize == 0) return nullptr;

    void* data = nullptr;
    if (runMode_ == ACL_HOST) {
        data = Utils::CopyDataDeviceToLocal(dataBufferDev, bufferSize);
        if (data == nullptr) return nullptr;
    } else {
        data = dataBufferDev;
    }
    itemDataSize = bufferSize;
    return data;
}

void ObjectDetect::DestroyResource() {
    if (imageDataBuf_) aclrtFree(imageDataBuf_);
    if (imageInfoBuf_) aclrtFree(imageInfoBuf_);
    
    model_.DestroyResource();
    aclrtResetDevice(deviceId_);
    aclFinalize();
}