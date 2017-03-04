LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= nv12rescale.cpp
LOCAL_SHARED_LIBRARIES := liblog libutils
LOCAL_MODULE:= libnv12rescale
include $(BUILD_SHARED_LIBRARY)
