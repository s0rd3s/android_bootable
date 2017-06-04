#
# Copyright (C) 2014 MediaTek Inc.
# Modification based on code covered by the mentioned copyright
# and/or permission notice(s).
#

LOCAL_SRC_FILES += \
	mt_install.c \
	mt_blockimg.c

ifeq ($(MTK_CACHE_MERGE_SUPPORT), true)
LOCAL_CFLAGS += -DCACHE_MERGE_SUPPORT
endif

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
LOCAL_CFLAGS += -DUBIFS_SUPPORT
LOCAL_STATIC_LIBRARIES += ubiutils
endif

ifeq ($(TARGET_ARCH),arm)
ifeq ($(strip $(MTK_FW_UPGRADE)), yes)
LOCAL_CFLAGS += -DMTK_SYS_FW_UPGRADE
LOCAL_STATIC_LIBRARIES += libfwupgrade
endif
endif

LOCAL_STATIC_LIBRARIES += libapplysig

