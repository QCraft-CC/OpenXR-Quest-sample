# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

# Declares and names the project.

project("xrtest")

get_filename_component(app_root ${CMAKE_SOURCE_DIR}/../../../.. ABSOLUTE)
set(xr_sdk ${app_root}/libs-3rdparty/openxr-sdk)
#add_subdirectory(${xr_sdk} openxr)

add_subdirectory(${app_root}/OpenComposite OpenComposite)

file(GLOB XR_SOURCE "hello_xr/*.cpp")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        native-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp

        #${XR_SOURCE}

        hello_openvr_vulkan/xrandroid.cpp
        hello_openvr_vulkan/hellovr_opengl_main.cpp
        # hello_openvr_vulkan/hellovr_vulkan_main.cpp
        hello_openvr_vulkan/shared/lodepng.cpp
        hello_openvr_vulkan/shared/Matrices.cpp
        hello_openvr_vulkan/shared/pathtools.cpp
        hello_openvr_vulkan/shared/strtools.cpp
        )

# Used by the OpenVR samples
target_compile_options(native-lib PRIVATE -DLINUX -DPOSIX)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

# Use OpenXR's GLES and EGL package files
set(CMAKE_MODULE_PATH "${xr_sdk}/src/cmake")

find_package(OpenGLES REQUIRED COMPONENTS V3)
find_package(EGL REQUIRED)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        native-lib

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}

        # headers
        )

# TODO load both of these properly via cmake
add_library(hacky_xr INTERFACE)
target_include_directories(hacky_xr INTERFACE ${xr_sdk}/include/)
target_compile_definitions(hacky_xr INTERFACE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)

target_link_libraries(native-lib hacky_xr)

# From openxrsdk/src/cmakelists.txt
# Do this in two parts: the header and the actual library itself
# The latter has to
find_path(ANDROID_NATIVE_APP_GLUE android_native_app_glue.h PATHS ${ANDROID_NDK}/sources/android/native_app_glue)
add_library(android_native_app_glue INTERFACE)
target_include_directories(android_native_app_glue INTERFACE ${ANDROID_NATIVE_APP_GLUE})

add_library(android_native_app_glue_obj OBJECT "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
target_include_directories(android_native_app_glue_obj PRIVATE ${ANDROID_NATIVE_APP_GLUE})
target_compile_options(android_native_app_glue_obj PRIVATE -Wno-unused-parameter)

target_link_libraries(native-lib android_native_app_glue $<TARGET_OBJECTS:android_native_app_glue_obj>)

# Add the openxr sample common stuff
file(GLOB XR_COMMON_SOURCE "hello_xr/common/*.c")
file(GLOB XR_COMMON_SOURCE_CPP "hello_xr/common/*.cpp")
add_library(xr_common STATIC ${XR_COMMON_SOURCE} ${XR_COMMON_SOURCE_CPP})
target_include_directories(xr_common PUBLIC hello_xr) # HACK! Required to make #include <common/abc.h> work
target_include_directories(xr_common PUBLIC hello_xr/external/include)
target_link_libraries(xr_common PUBLIC android_native_app_glue hacky_xr OpenGLES::OpenGLESv3 EGL android)

target_link_libraries(native-lib xr_common hacky_xr)

# Add the Oculus loader, and link the app against that
add_library(oculus_loader INTERFACE)
target_link_libraries(oculus_loader INTERFACE ${app_root}/libs-3rdparty/OculusOpenXR/OpenXR/Libs/Android/${ANDROID_ABI}/Release/libopenxr_loader.so)
target_include_directories(oculus_loader INTERFACE ${xr_sdk}/include)

target_link_libraries(native-lib oculus_loader)

# Link to OpenComposite
# Note we need Vulkan for the early pre-game-submits-frame state
target_link_libraries(native-lib OCOVR vulkan)
