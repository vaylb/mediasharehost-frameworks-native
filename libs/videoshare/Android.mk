LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
		DataBuffer.cpp \
		VideoShare.cpp \
		IVideoShare.cpp 

LOCAL_SHARED_LIBRARIES := \
	libui liblog libcutils libutils libbinder libjpeg libstagefright_foundation \

#LOCAL_WHOLE_STATIC_LIBRARIES = libjpeg_static
#LOCAL_STATIC_LIBRARIES := libsimd

LOCAL_MODULE:= libvideoshare

LOCAL_C_INCLUDES := \
		$(call include-path-for, audio-utils) \
		$(TOP)/external/jpeg \
#$(TOP)/frameworks/native/libs/libjpeg-turbo \

include $(BUILD_SHARED_LIBRARY)
