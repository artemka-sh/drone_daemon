#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <cstdio>
#include "acl/acl.h"

#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stderr, "[ERROR] " fmt "\n", ##args)

#define RGBU8_IMAGE_SIZE(width, height) ((width) * (height) * 3)

enum Result {
    SUCCESS = 0,
    FAILED = 1
};

class Utils {
public:
    static void* CopyDataDeviceToLocal(void* deviceData, uint32_t dataSize);
    static void* CopyDataToDevice(void* data, uint32_t dataSize, aclrtMemcpyKind policy);
    static void* CopyDataDeviceToDevice(void* deviceData, uint32_t dataSize);
    static void* CopyDataHostToDevice(void* deviceData, uint32_t dataSize);
};

#endif