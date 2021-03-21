#include "xrandroid.h"

// Access stuff from OpenComposite
#include "Misc/android_api.h"

#include <unistd.h>

/// ANDROIDY STUFF
// This is all copied and modified from hello_xr

android_app *current_app = nullptr;

struct AndroidAppState {
    bool Resumed;
    ANativeWindow *NativeWindow;
    IMainApplication *app;
};

static void app_handle_cmd(struct android_app *app, int32_t cmd) {
    AndroidAppState *appState = (AndroidAppState *) app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            log(ANDROID_LOG_INFO, "    APP_CMD_START");
            log(ANDROID_LOG_INFO, "onStart()");
            break;
        }
        case APP_CMD_RESUME: {
            log(ANDROID_LOG_INFO, "onResume()");
            log(ANDROID_LOG_INFO, "    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            log(ANDROID_LOG_INFO, "onPause()");
            log(ANDROID_LOG_INFO, "    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            log(ANDROID_LOG_INFO, "onStop()");
            log(ANDROID_LOG_INFO, "    APP_CMD_STOP");

            break;
        }
        case APP_CMD_DESTROY: {
            log(ANDROID_LOG_INFO, "onDestroy()");
            log(ANDROID_LOG_INFO, "    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            log(ANDROID_LOG_INFO, "surfaceCreated()");
            log(ANDROID_LOG_INFO, "    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            log(ANDROID_LOG_INFO, "surfaceDestroyed()");
            log(ANDROID_LOG_INFO, "    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
        default: {
            log(ANDROID_LOG_INFO, "Unknown APP_CMD: " + std::to_string((int) cmd));
            break;
        }
    }
}

// Our little hack to pass setup data to OC
XrInstanceCreateInfoAndroidKHR *OpenComposite_Android_Create_Info = nullptr;

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app *app) {
    XrInstanceCreateInfoAndroidKHR oc_info = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    OpenComposite_Android_Create_Info = &oc_info;

    current_app = app;

    try {
        AndroidAppState userData;
        memset(&userData, 0, sizeof(userData));
        app->userData = &userData;

        app->onAppCmd = app_handle_cmd;

        JNIEnv *Env;
        app->activity->vm->AttachCurrentThread(&Env, nullptr);

        bool requestRestart = false;
        bool exitRenderLoop = false;

        // Setup the info that OpenComposite will pass to the OpenXR runtime
        oc_info.applicationVM = app->activity->vm;
        oc_info.applicationActivity = app->activity->clazz;

        // Initialize the loader for this platform
        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        if (XR_SUCCEEDED(
                xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                      (PFN_xrVoidFunction *) (&initializeLoader)))) {
            XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
            memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
            loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
            loaderInitInfoAndroid.next = NULL;
            loaderInitInfoAndroid.applicationVM = app->activity->vm;
            loaderInitInfoAndroid.applicationContext = app->activity->clazz;
            initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *) &loaderInitInfoAndroid);
        }

        // Start the app
        userData.app = CreateApplication();
        if (!userData.app->BInit()) {
            log(ANDROID_LOG_ERROR, "Failed to BInit app");
            // Leaks resources, but who cares at this point
            return;
        }

        while (app->destroyRequested == 0) {
            // Read all pending events.
            for (;;) {
                int events;
                struct android_poll_source *source;
                // If the timeout is zero, returns immediately without blocking.
                // If the timeout is negative, waits indefinitely until an event appears.
                // const int timeoutMilliseconds =
                //         (!appState.Resumed && !program->IsSessionRunning() &&
                //          app->destroyRequested == 0) ? -1 : 0;
                const int timeoutMilliseconds = 0;
                if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void **) &source) < 0) {
                    break;
                }

                // Process this event.
                if (source != nullptr) {
                    source->process(app, source);
                }
            }

            // Throttle while not displaying anything
            if (!userData.Resumed) {
                usleep(100 * 1000);
                bool shouldExit = userData.app->SleepPoll();
                if (shouldExit) break;
                continue;
            }

            // Let the app render a frame, if it's active
            bool bQuit = userData.app->HandleInput();
            if (bQuit) break;

            userData.app->RenderFrame();
        }

        log(ANDROID_LOG_INFO, "Destroying app");
        userData.app->Shutdown();
        delete userData.app;
        userData.app = nullptr;

        app->activity->vm->DetachCurrentThread();
    } catch (const std::exception &ex) {
        log(ANDROID_LOG_ERROR, ex.what());
    } catch (...) {
        log(ANDROID_LOG_ERROR, "Unknown Error");
    }

    OpenComposite_Android_Create_Info = nullptr;
    current_app = nullptr;
}

std::vector<uint8_t> ReadAssetFile(std::string path) {
    log(ANDROID_LOG_INFO, "Load start for " + path);

    AAssetManager *am = current_app->activity->assetManager;
    AAsset *asset = AAssetManager_open(am, path.c_str(), AASSET_MODE_BUFFER);

    // Docs don't say that null=error but that seems logical
    if (!asset) {
        log(ANDROID_LOG_ERROR, "Failed to open asset: " + path);
        return std::vector<uint8_t>();
    }

    size_t length = AAsset_getLength(asset);
    const uint8_t *data = (const uint8_t *) AAsset_getBuffer(asset);

    std::vector<uint8_t> vec(data, data + length);
    AAsset_close(asset);

    log(ANDROID_LOG_INFO, "Done");

    return std::move(vec);
}

// Make the input manifest available from the assets
static std::string loadManifest(const char *path) {
    logf(ANDROID_LOG_INFO, "Load input file: '%s'", path);
    std::vector<uint8_t> data = ReadAssetFile(path);
    return std::string(data.data(), data.data() + data.size());
}

std::string (*OpenComposite_Android_Load_Input_File)(const char *path) = loadManifest;
