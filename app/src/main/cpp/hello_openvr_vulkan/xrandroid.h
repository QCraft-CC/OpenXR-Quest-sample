#pragma once

#include <android/log.h>
#include <android_native_app_glue.h>

#include <EGL/egl.h> // Required for openxr_platform
#include <openxr/openxr_platform.h>

#include <string>
#include <vector>

// SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);

class IMainApplication {
public:
    virtual ~IMainApplication() = default;

    virtual void Shutdown() = 0;

    virtual bool BInit() = 0;

    virtual bool HandleInput() = 0;

    virtual void RenderFrame() = 0;
};

IMainApplication *CreateApplication();

extern android_app *current_app;

#define SDL_MESSAGEBOX_ERROR 1
typedef uint32_t Uint32;

inline int SDL_ShowSimpleMessageBox(Uint32 flags,
                                    const char *title,
                                    const char *message,
                                    void *window) {

    android_LogPriority priority = ANDROID_LOG_INFO;
    if (flags & SDL_MESSAGEBOX_ERROR)
        android_LogPriority priority = ANDROID_LOG_ERROR;

    __android_log_print(priority, "HelloOpenVR", "msg: %s: %s", title, message);

    return 0;
}

inline void log(android_LogPriority priority, const std::string &msg) {
    __android_log_print(priority, "HelloOpenVR", "%s", msg.c_str());
}

inline void logf(android_LogPriority priority, const char *fmt, ...) {
    va_list args;
    char buffer[2048];

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    log(priority, buffer);
}

std::vector<uint8_t> ReadAssetFile(std::string path);

