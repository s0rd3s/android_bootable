/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#ifndef MT_RECOVERY_H_
#define MT_RECOVERY_H_

#include "device.h"

bool check_otaupdate_done(void);
void update_modem_property(void);

#ifdef __cplusplus
extern "C" {
#endif
bool is_support_nvdata(void);
#ifdef __cplusplus
}
#endif

int sdcard_restore_directory(const char *path, Device *device);

#ifdef SUPPORT_FOTA
extern "C" {
#include "fota/fota.h"
}
#endif

#ifdef ROOT_CHECK
#include "root_check.h"
extern "C" {
#include "cr32.h"
}
#endif

#ifdef MTK_SYS_FW_UPGRADE
extern "C"{
extern void fw_upgrade_finish();
extern void fw_upgrade(const char* path, int status);
}
extern int perform_update;
#endif

#ifdef SUPPORT_DATA_BACKUP_RESTORE //wschen 2011-03-09
#include "minzip/Zip.h"
#include "mt_check_partition.h"
#endif //SUPPORT_DATA_BACKUP_RESTORE

#ifdef SUPPORT_SBOOT_UPDATE
#include "sec/sec.h"
#endif

/* Variable body declared in mt_recovery.cpp */
extern const char *DEFAULT_MOTA_FILE;
extern const char *DEFAULT_FOTA_FOLDER;
extern const char *MOTA_RESULT_FILE;
extern const char *FOTA_RESULT_FILE;

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD)
extern const char *SDCARD2_ROOT;
#endif

#if defined(CACHE_MERGE_SUPPORT)
extern const char *DATA_CACHE_ROOT;
#endif

#ifdef SUPPORT_FOTA
extern const char *fota_delta_path;
#endif
#ifdef SUPPORT_DATA_BACKUP_RESTORE //wschen 2011-05-16
extern const char *restore_data_path;
#endif

/* Function body declared in mt_recovery.cpp */
bool remove_mota_file(const char *file_name);
void write_result_file(const char *file_name, int result);
void prompt_error_message(int reason);
void mt_copy_logs(void);
void mt_wipe_data(int confirm, Device* device);
void mt_set_sdcard_update_bootloader_message(void);

Device::BuiltinAction mt_prompt_and_wait(Device* device, int status);

extern "C"{
void write_all_log(void);
}

/* main(): MTK turnkey features */
int mt_main_arg(int arg);
int mt_main_init_sdcard2(void);
int mt_main_init_cache_merge(void);
const char *mt_main_init_sys_fw_upgrade(const char *update_package);
const char *mt_main_init_fota(const char *update_package);
int mt_main_update_package(int &status, const char *update_package, int wipe_cache);
int mt_main_wipe_data(int &status, Device* device, int wipe_cache);
int mt_main_wipe_cache(int &status);
const char *mt_main_fota(int &status);
const char *mt_main_backup_restore(int &status);
int mt_main_write_result(int &status, const char *update_package);
int mt_main_do_sys_fw_upgrade(const char *update_package);

/* main(): MTK turnkey arguments */
#define MT_OPTION_FOTA { "fota_delta_path", required_argument, NULL, 'f' },
#ifdef SUPPORT_DATA_BACKUP_RESTORE
#define MT_OPTION_BACKUP_RESTORE { "restore_data", required_argument, NULL, 'd' },
#else
#define MT_OPTION_BACKUP_RESTORE
#endif
#define MT_OPTION MT_OPTION_FOTA MT_OPTION_BACKUP_RESTORE

#endif

