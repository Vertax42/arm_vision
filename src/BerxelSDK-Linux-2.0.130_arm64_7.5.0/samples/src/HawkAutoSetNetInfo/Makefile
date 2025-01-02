
RELPATH_TO_ROOT = ..

ARCH = x86
include $(RELPATH_TO_ROOT)/Rules.mk


####################################################
####### Target Setting 
####################################################
LOCAL_EXECUTABLE_NAME := HawkAutoSetNetInfo
TARGET_EXECUTABLE_DIR := $(OUT_PARENT_DIR)
TARGET_EXECUTABLE_EXE := $(TARGET_EXECUTABLE_DIR)/$(LOCAL_EXECUTABLE_NAME)
LOCAL_SRC_FILES = main.cpp

LOCAL_INCLUDE_FILES = -I$(INCDIR) -I$(LOCAL_IN_FILES) -I./

all: $(TARGET_EXECUTABLE_DIR) $(TARGET_EXECUTABLE_EXE) 

$(TARGET_EXECUTABLE_EXE):$(LOCAL_SRC_FILES)
	g++ -std=c++11 $(LOCAL_SRC_FILES) -o $@ 

$(TARGET_EXECUTABLE_DIR):
	$(MKDIR) -p $(TARGET_EXECUTABLE_DIR)

clean:
	$(RM) $(TARGET_EXECUTABLE_EXE)

.PHONY: all clean
