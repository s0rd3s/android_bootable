/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
#ifndef MT_BLOCKIMG_H
#define MT_BLOCKIMG_H

#include "common.h"

#define SYSTEM_INDEX_BLOCK 0
#define CUSTOM_INDEX_BLOCK 1
#define VENDOR_INDEX_BLOCK 2

char *get_block_device(const char *partition);

#endif
