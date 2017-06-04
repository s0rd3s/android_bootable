/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#include "mt_blockimg.h"

char *get_block_device(const char *partition) {
    int fd = open("/dev/block/platform/mtk-msdc.0/by-name/para", O_RDONLY);
    if (fd == -1) {
        //DUMCHAR_INFO
        FILE *dum;
        char buf[512];
        char p_name[32], p_size[32], p_addr[32], p_actname[64];
        unsigned int p_type;
        char dev[3][64];
        dum = fopen("/proc/dumchar_info", "r");
        if (dum) {
            if (fgets(buf, sizeof(buf), dum) != NULL) {
                while (fgets(buf, sizeof(buf), dum)) {
                    if (sscanf(buf, "%s %s %s %d %s", p_name, p_size, p_addr, &p_type, p_actname) == 5) {
                        if (!strcmp(p_name, "bmtpool")) {
                            break;
                        }
                        if (!strcmp(p_name, "android")) {
                            snprintf(dev[SYSTEM_INDEX_BLOCK], sizeof(dev[SYSTEM_INDEX_BLOCK]), "%s", p_actname);
                        } else if (!strcmp(p_name, "custom")) {
                            snprintf(dev[CUSTOM_INDEX_BLOCK], sizeof(dev[CUSTOM_INDEX_BLOCK]), "%s", p_actname);
                        } else if (!strcmp(p_name, "vendor")) {
                            snprintf(dev[VENDOR_INDEX_BLOCK], sizeof(dev[VENDOR_INDEX_BLOCK]), "%s", p_actname);
                        }
                    }
                }
            }
            fclose(dum);
            if (!strcmp(partition, "system")) {
                return strdup(dev[SYSTEM_INDEX_BLOCK]);
            } else if (!strcmp(partition, "custom")) {
                return strdup(dev[CUSTOM_INDEX_BLOCK]);
            } else if (!strcmp(partition, "vendor")) {
                return strdup(dev[VENDOR_INDEX_BLOCK]);
            }
        } else {
            printf("open /proc/dumchar_info fail (%s)\n", strerror(errno));
        }
    } else {
        close(fd);
        //GPT
        if (!strcmp(partition, "system")) {
            return strdup(SYSTEM_PART);
        } else if (!strcmp(partition, "custom")) {
            return strdup(CUSTOM_PART);
        } else if (!strcmp(partition, "vendor")) {
            return strdup(VENDOR_PART);
        }
    }
    return NULL;
}
