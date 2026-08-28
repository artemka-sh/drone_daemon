#include "yolo12_detect.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

namespace {
// Те же 80 классов COCO, что и в object_detect.cpp. Если модель обучена
// на своих классах — замени список (и не забудь, что numClasses в
// Postprocess должен совпадать с nc, на котором обучалась модель).
const std::vector<std::string> cocoLabels = { "person", "bicycle", "car", "motorbike",
"aeroplane","bus", "train", "truck", "boat",
"traffic light", "fire hydrant", "stop sign", "parking meter",
"bench", "bird", "cat", "dog", "horse",
"sheep", "cow", "elephant", "bear", "zebra",
"giraffe", "backpack", "umbrella", "handbag","tie",
"suitcase", "frisbee", "skis", "snowboard", "sports ball",
"kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
"tennis racket", "bottle", "wine glass", "cup",
"fork", "knife", "spoon", "bowl", "banana",
"apple", "sandwich", "orange", "broccoli", "carrot",
"hot dog", "pizza", "donut", "cake", "chair",
"sofa", "potted plant", "bed", "dining table", "toilet",
"TV monitor", "laptop", "mouse", "remote", "keyboard",
"cell phone", "microwave", "oven", "toaster", "sink",
"refrigerator", "book", "clock", "vase","scissors",
"teddy bear", "hair drier", "toothbrush" };

struct RawDet {
    float x1, y1, x2, y2, score;
    int classId;
};

float IoU(const RawDet& a, const RawDet& b) {
    float xx1 = std::max(a.x1, b.x1), yy1 = std::max(a.y1, b.y1);
    float xx2 = std::min(a.x2, b.x2), yy2 = std::min(a.y2, b.y2);
    float w = std::max(0.f, xx2 - xx1), h = std::max(0.f, yy2 - yy1);
    float inter = w * h;
    float areaA = std::max(0.f, a.x2 - a.x1) * std::max(0.f, a.y2 - a.y1);
    float areaB = std::max(0.f, b.x2 - b.x1) * std::max(0.f, b.y2 - b.y1);
    float uni = areaA + areaB - inter;
    return uni > 0.f ? inter / uni : 0.f;
}

// Greedy NMS, подавление только внутри одного класса
std::vector<RawDet> NMS(std::vector<RawDet> dets, float iouThresh) {
    std::sort(dets.begin(), dets.end(), [](const RawDet& a, const RawDet& b) {
        return a.score > b.score;
    });
    std::vector<bool> removed(dets.size(), false);
    std::vector<RawDet> keep;
    for (size_t i = 0; i < dets.size(); ++i) {
        if (removed[i]) continue;
        keep.push_back(dets[i]);
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (removed[j] || dets[i].classId != dets[j].classId) continue;
            if (IoU(dets[i], dets[j]) > iouThresh) removed[j] = true;
        }
    }
    return keep;
}
} // namespace

Yolo12Detect::Yolo12Detect(const char* modelPath, uint32_t modelWidth, uint32_t modelHeight,
                           float confThresh, float iouThresh)
    : deviceId_(0), modelPath_(modelPath), modelWidth_(modelWidth), modelHeight_(modelHeight),
      imageDataBuf_(nullptr), isInited_(false), confThresh_(confThresh), iouThresh_(iouThresh) {
    imageDataSize_ = RGBU8_IMAGE_SIZE(modelWidth, modelHeight);
}

Yolo12Detect::~Yolo12Detect() {
    DestroyResource();
}

Result Yolo12Detect::InitResource() {
    const char* aclConfigPath = "../src/acl.json";
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

Result Yolo12Detect::InitModel(const char* omModelPath) {
    Result ret = model_.LoadModelFromFileWithMem(omModelPath);
    if (ret != SUCCESS) return FAILED;
    ret = model_.CreateDesc();
    if (ret != SUCCESS) return FAILED;
    ret = model_.CreateOutput();
    if (ret != SUCCESS) return FAILED;
    return SUCCESS;
}

Result Yolo12Detect::CreateModelInputDataset() {
    aclError aclRet = aclrtMalloc(&imageDataBuf_, imageDataSize_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (aclRet != ACL_ERROR_NONE) return FAILED;

    // У модели один вход "images". Нормализация (0..1) и BGR->RGB сделаны
    // через AIPP прямо в .om (см. aipp_yolo12.cfg) — сюда льём letterboxed
    // uint8 BGR картинку как есть, без ручной конвертации на CPU.
    Result ret = model_.CreateInput(imageDataBuf_, imageDataSize_);
    if (ret != SUCCESS) return FAILED;
    return SUCCESS;
}

Result Yolo12Detect::Init() {
    if (isInited_) return SUCCESS;
    if (InitResource() != SUCCESS) return FAILED;
    if (InitModel(modelPath_) != SUCCESS) return FAILED;
    if (CreateModelInputDataset() != SUCCESS) return FAILED;
    isInited_ = true;
    return SUCCESS;
}

Result Yolo12Detect::Preprocess(cv::Mat& frame) {
    origWidth_ = frame.cols;
    origHeight_ = frame.rows;

    // letterbox: сохраняем пропорции, добиваем серым (114,114,114) до
    // modelWidth_ x modelHeight_. Важно для точности — YOLO12 обучался
    // именно с такой геометрией входа (в отличие от простого squish-resize,
    // который был у старого ObjectDetect под YOLOv3).
    scale_ = std::min((float)modelWidth_ / origWidth_, (float)modelHeight_ / origHeight_);
    int newW = (int)std::round(origWidth_ * scale_);
    int newH = (int)std::round(origHeight_ * scale_);
    padLeft_ = ((int)modelWidth_ - newW) / 2;
    padTop_ = ((int)modelHeight_ - newH) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));

    cv::Mat letterboxed(modelHeight_, modelWidth_, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(letterboxed(cv::Rect(padLeft_, padTop_, newW, newH)));
    if (!letterboxed.isContinuous()) letterboxed = letterboxed.clone();

    aclrtMemcpyKind policy = (runMode_ == ACL_HOST) ? ACL_MEMCPY_HOST_TO_DEVICE : ACL_MEMCPY_DEVICE_TO_DEVICE;
    aclError ret = aclrtMemcpy(imageDataBuf_, imageDataSize_, letterboxed.ptr<uint8_t>(), imageDataSize_, policy);
    if (ret != ACL_ERROR_NONE) return FAILED;

    return SUCCESS;
}

Result Yolo12Detect::Inference(aclmdlDataset*& inferenceOutput) {
    Result ret = model_.Execute();
    if (ret != SUCCESS) return FAILED;
    inferenceOutput = model_.GetModelOutputData();
    return SUCCESS;
}

void* Yolo12Detect::GetInferenceOutputItem(uint32_t& itemDataSize, aclmdlDataset* inferenceOutput, uint32_t idx) {
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

Result Yolo12Detect::Postprocess(aclmdlDataset* modelOutput, std::vector<DetectionResult>& out_bboxes) {
    uint32_t dataSize = 0;
    float* raw = (float*)GetInferenceOutputItem(dataSize, modelOutput, 0);
    if (raw == nullptr) return FAILED;

    const size_t numClasses = cocoLabels.size(); // поменяй, если классы свои
    const size_t channels = 4 + numClasses;
    const size_t numAnchors = dataSize / sizeof(float) / channels;

    std::vector<RawDet> candidates;
    candidates.reserve(256);

    // Раскладка выхода — channel-major: canal c, anchor i -> raw[c*numAnchors + i]
    // (стандартный layout ONNX [1, 4+nc, N] после flatten).
    for (size_t i = 0; i < numAnchors; ++i) {
        float bestScore = 0.f;
        int bestClass = -1;
        for (size_t c = 0; c < numClasses; ++c) {
            float s = raw[(4 + c) * numAnchors + i];
            if (s > bestScore) { bestScore = s; bestClass = (int)c; }
        }
        if (bestScore < confThresh_) continue;

        float cx = raw[0 * numAnchors + i];
        float cy = raw[1 * numAnchors + i];
        float w  = raw[2 * numAnchors + i];
        float h  = raw[3 * numAnchors + i];

        RawDet d;
        d.x1 = cx - w / 2.f;
        d.y1 = cy - h / 2.f;
        d.x2 = cx + w / 2.f;
        d.y2 = cy + h / 2.f;
        d.score = bestScore;
        d.classId = bestClass;
        candidates.push_back(d);
    }

    auto kept = NMS(std::move(candidates), iouThresh_);

    out_bboxes.clear();
    for (auto& d : kept) {
        // снимаем letterbox-паддинг и масштаб — возвращаемся в координаты
        // исходного (не resize-нутого) кадра
        float x1 = (d.x1 - padLeft_) / scale_;
        float y1 = (d.y1 - padTop_) / scale_;
        float x2 = (d.x2 - padLeft_) / scale_;
        float y2 = (d.y2 - padTop_) / scale_;

        x1 = std::clamp(x1, 0.f, (float)origWidth_ - 1);
        y1 = std::clamp(y1, 0.f, (float)origHeight_ - 1);
        x2 = std::clamp(x2, 0.f, (float)origWidth_ - 1);
        y2 = std::clamp(y2, 0.f, (float)origHeight_ - 1);

        DetectionResult r;
        r.lt = { (uint32_t)x1, (uint32_t)y1 };
        r.rb = { (uint32_t)x2, (uint32_t)y2 };
        r.result_text = cocoLabels[d.classId] + " " + std::to_string((int)(d.score * 100)) + "%";
        out_bboxes.push_back(r);
    }

    if (runMode_ == ACL_HOST) {
        delete[]((uint8_t*)raw);
    }
    return SUCCESS;
}

void Yolo12Detect::DestroyResource() {
    if (imageDataBuf_) aclrtFree(imageDataBuf_);
    model_.DestroyResource();
    aclrtResetDevice(deviceId_);
    aclFinalize();
}
