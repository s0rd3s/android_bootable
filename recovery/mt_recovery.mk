#
# Copyright (C) 2014 MediaTek Inc.
# Modification based on code covered by the mentioned copyright
# and/or permission notice(s).
#

LOCAL_SRC_FILES += \
    mt_roots.cpp \
    mt_roots_ubi.cpp \
    mt_recovery.cpp \
    mt_bootloader.cpp \
    mt_install.cpp \
    utils/mt_check_partition.cpp \
    utils/mt_common_tk.cpp \
    utils/mt_sepol.cpp

ifeq ($(WITH_ROOT_CHECK),true)
LOCAL_SRC_FILES += \
    root_check.cpp \
    md5.c \
    cr32.c
endif

ifeq ($(WITH_FOTA),true)
ifeq ($(WITH_GPT_SCHEME), true)
LOCAL_SRC_FILES += \
    fota/fota_upg.cpp \
    fota/fota_fs.cpp \
    fota/fota_common.cpp
else
LOCAL_SRC_FILES += \
    fota/fota.cpp \
    fota/fota_fs.cpp \
    fota/fota_common.cpp \
    fota/fota_dev.cpp
endif
endif

ifeq ($(WITH_SBOOT_UPDATE),true)
LOCAL_SRC_FILES += \
    sec/sec.c
ifeq ($(CUSTOM_SEC_AUTH_SUPPORT),yes)
$(call config-custom-folder,custom:security/sbchk)
LOCAL_SRC_FILES += custom/cust_auth.c
else
LOCAL_SRC_FILES += auth/sec_wrapper.c
endif
endif

ifeq ($(WITH_BACKUP_RESTORE),true)
LOCAL_CFLAGS += -DSUPPORT_DATA_BACKUP_RESTORE
endif

ifeq ($(SPECIAL_FACTORY_RESET),true)
LOCAL_CFLAGS += -DSPECIAL_FACTORY_RESET
endif

ifeq ($(WITH_ROOT_CHECK),true)
LOCAL_CFLAGS += -DROOT_CHECK
ifeq ($(MTK_ATF_SUPPORT),yes)
LOCAL_CFLAGS += -DMTK_ATF_SUPPORT
endif
ifeq ($(TRUSTONIC_TEE_SUPPORT),yes)
LOCAL_CFLAGS += -DTRUSTONIC_TEE_SUPPORT
endif
endif

ifeq ($(WITH_FOTA),true)
LOCAL_CFLAGS += -DSUPPORT_FOTA -DFOTA_SELF_UPGRADE -DFOTA_PHONE_UPGRADE -DFOTA_UI_MESSAGE
LOCAL_CFLAGS += -fno-short-enums
#LOCAL_CFLAGS += -DVERIFY_BOOT_SOURCE -DVERIFY_BOOT_TARGET
#LOCAL_CFLAGS += -DVERIFY_SYSTEM_SOURCE -DVERIFY_SYSTEM_TARGET
#LOCAL_CFLAGS += -DVERIFY_RECOVERY_SOURCE -DVERIFY_RECOVERY_TARGET
#LOCAL_CFLAGS += -DVERIFY_CUSTOM_SOURCE -DVERIFY_CUSTOM_TARGET
endif

ifeq ($(WITH_SBOOT_UPDATE),true)
LOCAL_CFLAGS += -DSUPPORT_SBOOT_UPDATE
endif

ifeq ($(WITH_FOTA),true)
LOCAL_CFLAGS += -DFOTA_FIRST
endif

ifeq ($(MTK_SHARED_SDCARD),yes)
LOCAL_CFLAGS += -DMTK_SHARED_SDCARD
LOCAL_CFLAGS += -DSHARED_SDCARD
endif

ifeq ($(MTK_2SDCARD_SWAP),yes)
LOCAL_CFLAGS += -DMTK_2SDCARD_SWAP
endif

ifeq ($(CACHE_MERGE_SUPPORT),true)
LOCAL_CFLAGS += -DCACHE_MERGE_SUPPORT
endif

ifeq ($(MTK_GMO_ROM_OPTIMIZE),true)
LOCAL_CFLAGS += -DMTK_GMO_ROM_OPTIMIZE
endif


ifeq ($(TARGET_ARCH),arm)
ifeq ($(strip $(MTK_FW_UPGRADE)), yes)
LOCAL_CFLAGS += -DMTK_SYS_FW_UPGRADE
LOCAL_STATIC_LIBRARIES += libfwupgrade
endif
endif

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
LOCAL_CFLAGS += -DUBIFS_SUPPORT
LOCAL_STATIC_LIBRARIES += ubi_ota_update
endif

ifeq ($(PURE_AP_USE_EXTERNAL_MODEM),yes)
LOCAL_CFLAGS += -DEXTERNAL_MODEM_UPDATE
endif

#add for fat merge
ifeq ($(MTK_MLC_NAND_SUPPORT),yes)
LOCAL_CFLAGS += -DBOARD_UBIFS_FAT_MERGE_VOLUME_SIZE=$(BOARD_UBIFS_FAT_MERGE_VOLUME_SIZE)
LOCAL_CFLAGS += -DBOARD_UBIFS_IPOH_VOLUME_SIZE=$(BOARD_UBIFS_IPOH_VOLUME_SIZE)
endif

ifeq ($(WITH_BACKUP_RESTORE),true)
LOCAL_STATIC_LIBRARIES += libbackup_restore libcrypto_static libselinux libsepol
LOCAL_C_INCLUDES += external/libselinux/include
endif


ifeq ($(WITH_FOTA),true)
ifeq ($(TARGET_ARCH),arm64)
LOCAL_STATIC_LIBRARIES += upi_v9_64
else
LOCAL_STATIC_LIBRARIES += upi_v9
endif
endif


ifeq ($(WITH_SBOOT_UPDATE),true)
ifeq ($(TARGET_ARCH),arm64)
    LOCAL_STATIC_LIBRARIES += libsbup
    LOCAL_STATIC_LIBRARIES += libsbauth
else
    ifeq ($(WITH_GPT_SCHEME),true)
        LOCAL_STATIC_LIBRARIES += libsbup_gpt
    else
        LOCAL_STATIC_LIBRARIES += libsbup_legacy
    endif

    ifneq ($(CUSTOM_SEC_AUTH_SUPPORT),yes)
        ifeq ($(WITH_GPT_SCHEME),true)
            LOCAL_STATIC_LIBRARIES += libsbauth_gpt
        else
            LOCAL_STATIC_LIBRARIES += libsbauth_legacy
        endif
    endif
endif
endif

LOCAL_C_INCLUDES += kernel
LOCAL_C_INCLUDES += $(LOCAL_PATH)/fota/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/utils/include
LOCAL_C_INCLUDES += \
    external/libselinux/include \
    external/libsepol/include \
    bionic/libc
