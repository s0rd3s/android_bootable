LOCAL_DIR := $(GET_LOCAL_DIR)

MODULES += \
	$(LOCAL_DIR)/video \
	$(LOCAL_DIR)/lcm

ifeq ($(MTK_LOG_SAVE_SUPPORT), yes)
    MODULES += $(LOCAL_DIR)/logsave
endif

OBJS += \
	$(LOCAL_DIR)/dev.o

include $(LOCAL_DIR)/logo/rules.mk
