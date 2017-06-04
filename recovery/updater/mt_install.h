/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
#ifndef MT_INSTALL_H
#define MT_INSTALL_H

#include <time.h>
#include <stdbool.h>
#include "applysig/applysig.h"
#include "libubi.h"
#include "ubiutils-common.h"
#include "edify/expr.h"

/* Link to bionic/libc/include/util.h but it is empty file */
//#include "util.h"
#include "roots.h"
#include "common.h"

#define CACHE_INDEX  0
#define DATA_INDEX   1
#define SYSTEM_INDEX 2
#define CUSTOM_INDEX 3

/* Variables in mt_install.c */
extern int phone_type;
extern int is_gpt;

/* Common */
int remove_dir(const char *dirname);
void mt_init_partition_type(void);
char *mt_get_location(char *mount_point);

/* UnmountFn() */
int mt_UnmountFn_chache_merege(char **mount_point, char **result);
int mt_UnmountFn_ubifs(char *mount_point);

/* MountFn() */
int mt_MountFn(char **mount_point, char **result, char **fs_type, char **location,
		char **partition_type, const char *name, char* mount_options, bool has_mount_options, State* state);
int	mt_MountFn_cache_merge_init(char **mount_point, char **result);
int	mt_MountFn_cache_merge_final(char **mount_point, char **result);

/* FormatFn() */
int mt_FormatFn(char **mount_point, char **result, char **fs_type, char **location,
	char **partition_type, const char *name, const char *fs_size);

/* WriteRawImageFn() */
int mt_WriteRawImageFn(State* state, char **partition, char **result, Value **partition_value, Value **contents);

/* PackageExtractFileFn() */
Value* mt_PackageExtractFileFn_ubifs(const char *name, const char *dest_path,
	const char *zip_path, ZipArchive* za, const ZipEntry* entry, Value **retval, bool *success);

/* RebootNowFn() */
Value* mt_RebootNowFn(const char* name, State* state, char** filename);

/* SetStageFn(), GetStageFn() */
Value* mt_SetStageFn(const char* name, State* state, char** filename, char** stagestr);
Value* mt_GetStageFn(const char* name, State* state, char** filename, char *buffer);

/* Register Functions */
void mt_RegisterInstallFunctions(void);

#endif

