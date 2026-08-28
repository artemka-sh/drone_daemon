#pragma once
#include <iostream>
#include "utils.h"
#include "acl/acl.h"

class ModelProcess {
    public:
    ModelProcess();
    ~ModelProcess();

    Result LoadModelFromFileWithMem(const char *modelPath);
    void DestroyResource();
    void Unload();
    Result CreateDesc();
    void DestroyDesc();

    // Исходная перегрузка под модели с двумя входами (напр. YOLOv3-сэмпл:
    // картинка + imageInfo)
    Result CreateInput(void *input1, size_t input1size,
    void* input2, size_t input2size);

    // НОВОЕ: перегрузка под модели с одним входом (YOLO12 и вообще большинство
    // ONNX-детекторов без встроенного постпроцессинга)
    Result CreateInput(void *input, size_t inputSize);

    void DestroyInput();
    Result CreateOutput();
    void DestroyOutput();
    Result Execute();
    aclmdlDataset *GetModelOutputData();

    private:
    bool loadFlag_;
    uint32_t modelId_;
    void *modelMemPtr_;
    size_t modelMemSize_;
    void *modelWeightPtr_;
    size_t modelWeightSize_;
    aclmdlDesc *modelDesc_;
    aclmdlDataset *input_;
    aclmdlDataset *output_;

    bool isReleased_;
};
