
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := liblive555
LOCAL_SRC_FILES := prebuilt/liblive555.so

include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/live/liveMedia/include \
	$(LOCAL_PATH)/live/BasicUsageEnvironment/include \
	$(LOCAL_PATH)/live/groupsock/include \
	$(LOCAL_PATH)/live/UsageEnvironment/include  
	
LOCAL_SHARED_LIBRARIES :=  liblive555
LOCAL_LDLIBS    := -llog
	
LOCAL_MODULE    := mjpeg_live
LOCAL_SRC_FILES := output_live.cpp


STL_PATH=$(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a
LOCAL_LDLIBS += -L$(STL_PATH) -lsupc++

include $(BUILD_SHARED_LIBRARY)




