LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Module name
LOCAL_MODULE := rmlui_core

# Source files
LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
    $(wildcard $(LOCAL_PATH)/Source/Core/*.cpp) \
    $(wildcard $(LOCAL_PATH)/Source/Core/Elements/*.cpp) \
    $(wildcard $(LOCAL_PATH)/Source/Core/FontEngineDefault/*.cpp) \
    $(wildcard $(LOCAL_PATH)/Source/Core/Layout/*.cpp))

# Include directories
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/Include \
	$(LOCAL_PATH)/../freetype/include \

# C++ flags
LOCAL_CPPFLAGS := -std=c++17 -DRMLUI_STATIC_LIB -DRMLUI_SDL_VERSION_MAJOR=3 -DRMLUI_FONT_ENGINE_FREETYPE -DRMLUI_BACKEND_SIMULATE_TOUCH

# Optional: enable RTTI / exceptions if required
LOCAL_CPP_FEATURES := rtti exceptions

# Build as static library (change to BUILD_SHARED_LIBRARY if needed)
include $(BUILD_STATIC_LIBRARY)
