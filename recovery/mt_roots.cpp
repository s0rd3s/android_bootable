/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "roots.h"
#include "common.h"
#include "mt_partition.h"

static int num_volumes = 0;
static Volume* device_volumes = NULL;
static const char* PERSISTENT_PATH = "/persistent";
static const char* NVDATA_PATH = "/nvdata";

#if defined(CACHE_MERGE_SUPPORT)
#include <dirent.h>
#include "mt_check_partition.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static int remove_dir(const char *dirname)
{
    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];

    dir = opendir(dirname);
    if (dir == NULL) {
        LOGE("opendir %s failed\n", dirname);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            snprintf(path, (size_t) PATH_MAX, "%s/%s", dirname, entry->d_name);
            if (entry->d_type == DT_DIR) {
                remove_dir(path);
            } else {
                // delete file
                unlink(path);
            }
        }
    }
    closedir(dir);

    // now we can delete the empty dir
    rmdir(dirname);
    return 0;
}
#endif

static int parse_options(char* options, Volume* volume) {
    char* option;
    while ((option = strtok(options, ","))) {
        options = NULL;

        if (strncmp(option, "length=", 7) == 0) {
            volume->length = strtoll(option+7, NULL, 10);
        } else {
            LOGE("bad option \"%s\"\n", option);
            return -1;
        }
    }
    return 0;
}

Volume* mt_volume_for_path(const char* path) {
    int i;
#if defined(CACHE_MERGE_SUPPORT)
    char *search_path;

    // replace /cache to DATA_CACHE_ROOT (return data volume)
    if (!strncmp(path, "/cache", strlen("/cache"))) {
        search_path = (char *)DATA_CACHE_ROOT;
    } else {
        search_path = (char *)path;
    }

    for (i = 0; i < num_volumes; ++i) {
        Volume* v = device_volumes+i;
        int len = strlen(v->mount_point);
        if (strncmp(search_path, v->mount_point, len) == 0 &&
            (search_path[len] == '\0' || search_path[len] == '/')) {
            return v;
        }
    }
#else
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = device_volumes+i;
        int len = strlen(v->mount_point);
        if (strncmp(path, v->mount_point, len) == 0 &&
            (path[len] == '\0' || path[len] == '/')) {
            return v;
        }
    }
#endif
    return NULL;
}

void mt_ensure_dev_ready(const char *dev_path) {
    int count=0;
    if (dev_path) {
        while ((count++ < 5) && (access(dev_path, R_OK) != 0)) {
            printf("no %s entry , count = %d\n", dev_path, count);
            sleep(1);
        }
    } else {
        printf("Error: Retry fail %s not found\n",dev_path);
    }
}

int mt_setup_install_mounts(void)
{
    for (int i = 0; i < num_volumes; ++i) {
        if (strcmp(device_volumes[i].mount_point, "/tmp") == 0 ||
                strcmp(device_volumes[i].mount_point, "/cache") == 0) {
            if (ensure_path_mounted(device_volumes[i].mount_point) != 0) {
                LOGE("failed to mount %s\n", device_volumes[i].mount_point);
                return -1;
            }

        } else {
            if (ensure_path_unmounted(device_volumes[i].mount_point) != 0) {
                LOGE("failed to unmount %s\n", device_volumes[i].mount_point);
                return -1;
            }
        }
    }
    return 0;
}

void mt_load_volume_table(void)
{
    int alloc = 2;
    device_volumes = (Volume*)malloc(alloc * sizeof(Volume));

    // Insert an entry for /tmp, which is the ramdisk and is always mounted.
    device_volumes[0].mount_point = "/tmp";
    device_volumes[0].fs_type = "ramdisk";
    device_volumes[0].blk_device = NULL;
    device_volumes[0].blk_device2 = NULL;
    device_volumes[0].length = 0;
    num_volumes = 1;

#if 1 //wschen 2011-12-23 auto workaround if recovery.fstab is wrong, phone is eMMC but recovery.fstab is NAND
#define NAND_TYPE    0
#define EMMC_TYPE    1
#define UNKNOWN_TYPE 2

#define CACHE_INDEX    0
#define DATA_INDEX     1
#define SYSTEM_INDEX   2
#define FAT_INDEX      3
#define CUSTOM_INDEX   4
#define PERSIST_INDEX  5
#define MAX_PART_INDEX 6

    int setting_ok = 0;
    int phone_type = 0, fstab_type = UNKNOWN_TYPE;
    int has_fat = 0;
    FILE *fp_fstab, *fp_info;
    char buf[512];
    char p_name[32], p_size[32], p_addr[32], p_actname[64];
    char dev[MAX_PART_INDEX][64];
    unsigned int p_type;
    int j;
    int has_custom = 0;
    int new_fstab = 0;
    int has_persist = 0;
    int has_nvdata = 0;
    part_detail_info part_info;

    bool is_gpt = is_support_gpt();

    if (is_gpt) {
        //GPT supported
       printf("Partition Information:\n");
       for(j=1; j<=MAX_PARTITION_NUM; j++)
       {
           if(parse_partition_info_by_index(j, &part_info))  {

             printf("%15s    0x%016llx  0x%016llx \n",part_info.part_name,part_info.part_start_addr,part_info.part_size);

             if (!strcmp(part_info.part_name, "intsd")) {
                has_fat = 1;
             } else if (!strcasecmp(part_info.part_name, "persist")) {
                has_persist = 1;
             } else if (!strcasecmp(part_info.part_name, "nvdata")) {
                has_nvdata = 1;
             } else if (!strcasecmp(part_info.part_name, "custom")) {
                has_custom = 1;
             }
           }
       }
#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15
            if (has_custom) {
                device_volumes = (Volume*)realloc(device_volumes, 10 * sizeof(Volume));
                num_volumes = 10;
            } else {
                device_volumes = (Volume*)realloc(device_volumes, 9 * sizeof(Volume));
                num_volumes = 9;
            }
#else
            if (has_custom) {
                device_volumes = (Volume*)realloc(device_volumes, 9 * sizeof(Volume));
                num_volumes = 9;
            } else {
                device_volumes = (Volume*)realloc(device_volumes, 8 * sizeof(Volume));
                num_volumes = 8;
            }
#endif //SUPPORT_SDCARD2

            if (has_persist) {
                num_volumes ++;
                device_volumes = (Volume*)realloc(device_volumes, num_volumes * sizeof(Volume));
            }

            if (has_nvdata) {
                num_volumes ++;
                device_volumes = (Volume*)realloc(device_volumes, num_volumes * sizeof(Volume));
            }

            //boot
            device_volumes[1].mount_point = strdup("/boot");
            device_volumes[1].fs_type = strdup("emmc");
            device_volumes[1].blk_device = strdup(BOOT_PART);
            device_volumes[1].blk_device2 = NULL;
            device_volumes[1].length = 0;

            //cache
#if defined(CACHE_MERGE_SUPPORT)
            device_volumes[2].mount_point = strdup("/.cache");
#else
            device_volumes[2].mount_point = strdup("/cache");
#endif
            device_volumes[2].fs_type = strdup("ext4");
            device_volumes[2].blk_device = strdup(CACHE_PART);
            device_volumes[2].blk_device2 = NULL;
            device_volumes[2].length = 0;

            //data
            device_volumes[3].mount_point = strdup("/data");
            device_volumes[3].fs_type = strdup("ext4");
            device_volumes[3].blk_device = strdup(DATA_PART);
            device_volumes[3].blk_device2 = NULL;
            device_volumes[3].length = 0;

            //misc
            device_volumes[4].mount_point = strdup("/misc");
            device_volumes[4].fs_type = strdup("emmc");
            device_volumes[4].blk_device = strdup(MISC_PART);
            device_volumes[4].blk_device2 = NULL;
            device_volumes[4].length = 0;

            //recovery
            device_volumes[5].mount_point = strdup("/recovery");
            device_volumes[5].fs_type = strdup("emmc");
            device_volumes[5].blk_device = strdup(RECOVERY_PART);
            device_volumes[5].blk_device2 = NULL;
            device_volumes[5].length = 0;

            //sdcard
            if (has_fat) {
                device_volumes[6].mount_point = strdup("/sdcard");
                device_volumes[6].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_SHARED_SDCARD) || defined(MTK_2SDCARD_SWAP)
                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                device_volumes[6].blk_device2 = strdup(FAT_PART);
#else
                device_volumes[6].blk_device = strdup(FAT_PART);
                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1p1");
#endif
#endif // not MTK_GMO_ROM_OPTIMIZE
                device_volumes[6].length = 0;
            } else {
                device_volumes[6].mount_point = strdup("/sdcard");
                device_volumes[6].fs_type = strdup("vfat");
                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1");
                device_volumes[6].length = 0;
            }

            //system
            device_volumes[7].mount_point = strdup("/system");
            device_volumes[7].fs_type = strdup("ext4");
            device_volumes[7].blk_device = strdup(SYSTEM_PART);
            device_volumes[7].blk_device2 = NULL;
            device_volumes[7].length = 0;

            if (has_custom) {
                //custom
                device_volumes[8].mount_point = strdup("/custom");
                device_volumes[8].fs_type = strdup("ext4");
                device_volumes[8].blk_device = strdup(CUSTOM_PART);
                device_volumes[8].blk_device2 = NULL;
                device_volumes[8].length = 0;
            }

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15
            if (has_custom) {
                //sdcard2
                if (has_fat) {
                    device_volumes[9].mount_point = strdup("/sdcard2");
                    device_volumes[9].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                    device_volumes[9].blk_device = strdup("/dev/block/mmcblk1p1");
                    device_volumes[9].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_2SDCARD_SWAP)
                    device_volumes[9].blk_device = strdup(FAT_PART);
                    device_volumes[9].blk_device2 = NULL;
#else
                    device_volumes[9].blk_device = strdup("/dev/block/mmcblk1p1");
                    device_volumes[9].blk_device2 = strdup("/dev/block/mmcblk1");
#endif
#endif
                    device_volumes[9].length = 0;
                } else {
                    //no 2nd SD
                    num_volumes--;
                } // has_fat
            } else {
                //not has_custom
                //sdcard2
                if (has_fat) {
                    device_volumes[8].mount_point = strdup("/sdcard2");
                    device_volumes[8].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                    device_volumes[8].blk_device = strdup("/dev/block/mmcblk1p1");
                    device_volumes[8].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_2SDCARD_SWAP)
                    device_volumes[8].blk_device = strdup(FAT_PART);
                    device_volumes[8].blk_device2 = NULL;
#else
                    device_volumes[8].blk_device = strdup("/dev/block/mmcblk1p1");
                    device_volumes[8].blk_device2 = strdup("/dev/block/mmcblk1");
#endif
#endif
                    device_volumes[8].length = 0;
                } else {
                    //no 2nd SD
                    num_volumes--;
                } // has_fat
            }
#endif //SUPPORT_SDCARD2

            if(has_persist && has_nvdata) {
                device_volumes[num_volumes - 2].mount_point = strdup(PERSISTENT_PATH);
                device_volumes[num_volumes - 2].fs_type = strdup("ext4");
                device_volumes[num_volumes - 2].blk_device = strdup(PERSIST_PART);
                device_volumes[num_volumes - 2].blk_device2 = NULL;
                device_volumes[num_volumes - 2].length = 0;

                device_volumes[num_volumes - 1].mount_point = strdup(NVDATA_PATH);
                device_volumes[num_volumes - 1].fs_type = strdup("ext4");
                device_volumes[num_volumes - 1].blk_device = strdup(NVDATA_PART);
                device_volumes[num_volumes - 1].blk_device2 = NULL;
                device_volumes[num_volumes - 1].length = 0;

            } else {
                if (has_persist) {
                    device_volumes[num_volumes - 1].mount_point = strdup(PERSISTENT_PATH);
                    device_volumes[num_volumes - 1].fs_type = strdup("ext4");
                    device_volumes[num_volumes - 1].blk_device = strdup(PERSIST_PART);
                    device_volumes[num_volumes - 1].blk_device2 = NULL;
                    device_volumes[num_volumes - 1].length = 0;
                }

                if (has_nvdata) {
                    device_volumes[num_volumes - 1].mount_point = strdup(NVDATA_PATH);
                    device_volumes[num_volumes - 1].fs_type = strdup("ext4");
                    device_volumes[num_volumes - 1].blk_device = strdup(NVDATA_PART);
                    device_volumes[num_volumes - 1].blk_device2 = NULL;
                    device_volumes[num_volumes - 1].length = 0;
                }
            }

            mt_ensure_dev_ready(MISC_PART);
            mt_ensure_dev_ready(CACHE_PART);

            printf("recovery filesystem table\n");
            printf("=========================\n");
            for (j = 0; j < num_volumes; ++j) {
                Volume* v = &device_volumes[j];
                printf("  %d %s %s %s %s %lld\n", j, v->mount_point, v->fs_type, v->blk_device, v->blk_device2, v->length);
            }
            printf("\n");
            return;

    } else {
        //dumchar_info supported
        fp_info = fopen("/proc/dumchar_info", "r");
        if (fp_info) {
            //ignore the header line
            if (fgets(buf, sizeof(buf), fp_info) != NULL) {
                printf("Partition Information:\n");
                while (fgets(buf, sizeof(buf), fp_info)) {
                    printf("%s", buf);
                    if (sscanf(buf, "%s %s %s %d %s", p_name, p_size, p_addr, &p_type, p_actname) == 5) {
                        if (!strcmp(p_name, "bmtpool")) {
                            break;
                        }
                        if (!strcmp(p_name, "preloader")) {
                            if (p_type == 2) {
                                phone_type = EMMC_TYPE;
                            } else {
                                phone_type = NAND_TYPE;
                            }
                        } else if (!strcmp(p_name, "fat")) {
                            has_fat = 1;
                            snprintf(dev[FAT_INDEX], sizeof(dev[FAT_INDEX]), "%s", p_actname);
                        } else if (!strcmp(p_name, "cache")) {
                            snprintf(dev[CACHE_INDEX], sizeof(dev[CACHE_INDEX]), "%s", p_actname);
                        } else if (!strcmp(p_name, "persist")) {
                            snprintf(dev[PERSIST_INDEX], sizeof(dev[PERSIST_INDEX]), "%s", p_actname);
                            has_persist = 1;
                        } else if (!strcmp(p_name, "usrdata")) {
                            snprintf(dev[DATA_INDEX], sizeof(dev[DATA_INDEX]), "%s", p_actname);
                        } else if (!strcmp(p_name, "android")) {
                            snprintf(dev[SYSTEM_INDEX], sizeof(dev[SYSTEM_INDEX]), "%s", p_actname);
                        } else if (!strcmp(p_name, "custom")) {
                            snprintf(dev[CUSTOM_INDEX], sizeof(dev[CUSTOM_INDEX]), "%s", p_actname);
                            has_custom = 1;
                        }
                    }
                }
            }
            fclose(fp_info);
            printf("\n");

            fp_fstab = fopen("/etc/recovery.fstab", "r");
            if (fp_fstab) {
                while (fgets(buf, sizeof(buf), fp_fstab)) {
                    for (j = 0; buf[j] && isspace(buf[j]); ++j) {
                    }

                    if (buf[j] == '\0' || buf[j] == '#') {
                        continue;
                    }
                    if (strstr(&buf[j], "/boot") == (&buf[j])) {
                        j += strlen("/boot");
                        for (; buf[j] && isspace(buf[j]); ++j) {
                        }
                        if (strstr(&buf[j], "emmc") == (&buf[j])) {
                            fstab_type = EMMC_TYPE;
                        } else if (strstr(&buf[j], "mtd") == (&buf[j])) {
                            fstab_type = NAND_TYPE;
                        }
                        break;
                    } else if (strstr(&buf[j], "boot") == (&buf[j])) {
                        //tonykuo 2013-11-28
                        new_fstab = 1;
                        j += strlen("boot");
                        for (; buf[j] && isspace(buf[j]); ++j) {
                        }
                        if (strstr(&buf[j], "/boot") == (&buf[j])) {
                            j += strlen("/boot");
                            for (; buf[j] && isspace(buf[j]); ++j) {
                            }
                            if (strstr(&buf[j], "emmc") == (&buf[j])) {
                                fstab_type = EMMC_TYPE;
                            } else if (strstr(&buf[j], "mtd") == (&buf[j])) {
                                fstab_type = NAND_TYPE;
                            }
                            break;
                        }
                    }
                }

                fclose(fp_fstab);

                if (fstab_type != UNKNOWN_TYPE) {
                    if (phone_type != fstab_type) {
                        printf("WARNING : fstab is wrong, phone=%s fstab=%s\n", phone_type == EMMC_TYPE ? "eMMC" : "NAND", fstab_type == EMMC_TYPE ? "eMMC" : "NAND");

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15
                        if (has_custom) {
                            device_volumes = (Volume*)realloc(device_volumes, 10 * sizeof(Volume));
                            num_volumes = 10;
                        } else {
                            device_volumes = (Volume*)realloc(device_volumes, 9 * sizeof(Volume));
                            num_volumes = 9;
                        }
#else
                        if (has_custom) {
                            device_volumes = (Volume*)realloc(device_volumes, 9 * sizeof(Volume));
                            num_volumes = 9;
                        } else {
                            device_volumes = (Volume*)realloc(device_volumes, 8 * sizeof(Volume));
                            num_volumes = 8;
                        }
#endif //SUPPORT_SDCARD2

                        if (has_persist) {
                            num_volumes++;
                            device_volumes = (Volume*)realloc(device_volumes, num_volumes * sizeof(Volume));
                        }

                        if (phone_type == EMMC_TYPE) {
                            //boot
                            device_volumes[1].mount_point = strdup("/boot");
                            device_volumes[1].fs_type = strdup("emmc");
                            device_volumes[1].blk_device = strdup("boot");
                            device_volumes[1].blk_device2 = NULL;
                            device_volumes[1].length = 0;

                            //cache
#if defined(CACHE_MERGE_SUPPORT)
                            device_volumes[2].mount_point = strdup("/.cache");
#else
                            device_volumes[2].mount_point = strdup("/cache");
#endif
                            device_volumes[2].fs_type = strdup("ext4");
                            device_volumes[2].blk_device = strdup(dev[CACHE_INDEX]);
                            device_volumes[2].blk_device2 = NULL;
                            device_volumes[2].length = 0;

                            //data
                            device_volumes[3].mount_point = strdup("/data");
                            device_volumes[3].fs_type = strdup("ext4");
                            device_volumes[3].blk_device = strdup(dev[DATA_INDEX]);
                            device_volumes[3].blk_device2 = NULL;
                            device_volumes[3].length = 0;

                            //misc
                            device_volumes[4].mount_point = strdup("/misc");
                            device_volumes[4].fs_type = strdup("emmc");
                            device_volumes[4].blk_device = strdup("misc");
                            device_volumes[4].blk_device2 = NULL;
                            device_volumes[4].length = 0;

                            //recovery
                            device_volumes[5].mount_point = strdup("/recovery");
                            device_volumes[5].fs_type = strdup("emmc");
                            device_volumes[5].blk_device = strdup("recovery");
                            device_volumes[5].blk_device2 = NULL;
                            device_volumes[5].length = 0;

                            //sdcard
                            if (has_fat) {
                                device_volumes[6].mount_point = strdup("/sdcard");
                                device_volumes[6].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_SHARED_SDCARD) || defined(MTK_2SDCARD_SWAP)
                                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                                device_volumes[6].blk_device2 = strdup(dev[FAT_INDEX]);
#else
                                device_volumes[6].blk_device = strdup(dev[FAT_INDEX]);
                                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1p1");
#endif
#endif
                                device_volumes[6].length = 0;
                            } else {
                                device_volumes[6].mount_point = strdup("/sdcard");
                                device_volumes[6].fs_type = strdup("vfat");
                                device_volumes[6].blk_device = strdup("/dev/block/mmcblk1p1");
                                device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk1");
                                device_volumes[6].length = 0;
                            }

                            //system
                            device_volumes[7].mount_point = strdup("/system");
                            device_volumes[7].fs_type = strdup("ext4");
                            device_volumes[7].blk_device = strdup(dev[SYSTEM_INDEX]);
                            device_volumes[7].blk_device2 = NULL;
                            device_volumes[7].length = 0;

                            if (has_custom) {
                                //custom
                                device_volumes[8].mount_point = strdup("/custom");
                                device_volumes[8].fs_type = strdup("ext4");
                                device_volumes[8].blk_device = strdup(dev[CUSTOM_INDEX]);
                                device_volumes[8].blk_device2 = NULL;
                                device_volumes[8].length = 0;
                            }

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15
                            if (has_custom) {
                                //sdcard2
                                if (has_fat) {
                                    device_volumes[9].mount_point = strdup("/sdcard2");
                                    device_volumes[9].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                                    device_volumes[9].blk_device = strdup("/dev/block/mmcblk1p1");
                                    device_volumes[9].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_2SDCARD_SWAP)
                                    device_volumes[9].blk_device = strdup(dev[FAT_INDEX]);
                                    device_volumes[9].blk_device2 = NULL;
#else
                                    device_volumes[9].blk_device = strdup("/dev/block/mmcblk1p1");
                                    device_volumes[9].blk_device2 = strdup("/dev/block/mmcblk1");
#endif
#endif
                                    device_volumes[9].length = 0;
                                } else {
                                    //no 2nd SD
                                    num_volumes--;
                                } // has_fat
                            } else {
                                //sdcard2
                                if (has_fat) {
                                    device_volumes[8].mount_point = strdup("/sdcard2");
                                    device_volumes[8].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
                                    device_volumes[8].blk_device = strdup("/dev/block/mmcblk1p1");
                                    device_volumes[8].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_2SDCARD_SWAP)
                                    device_volumes[8].blk_device = strdup(dev[FAT_INDEX]);
                                    device_volumes[8].blk_device2 = NULL;
#else
                                    device_volumes[8].blk_device = strdup("/dev/block/mmcblk1p1");
                                    device_volumes[8].blk_device2 = strdup("/dev/block/mmcblk1");
#endif
#endif
                                    device_volumes[8].length = 0;
                                } else {
                                    //no 2nd SD
                                    num_volumes--;
                                } // has_fat

                            }
#endif //SUPPORT_SDCARD2
                            if (has_persist) {
                                device_volumes[num_volumes - 1].mount_point = strdup(PERSISTENT_PATH);
                                device_volumes[num_volumes - 1].fs_type = strdup("ext4");
                                device_volumes[num_volumes - 1].blk_device = strdup(dev[PERSIST_INDEX]);
                                device_volumes[num_volumes - 1].blk_device2 = NULL;
                                device_volumes[num_volumes - 1].length = 0;
                            }

                        } else {
                            //boot
                            device_volumes[1].mount_point = strdup("/boot");
                            device_volumes[1].fs_type = strdup("mtd");
                            device_volumes[1].blk_device = strdup("boot");
                            device_volumes[1].blk_device2 = NULL;
                            device_volumes[1].length = 0;

                            //cache
#if defined(CACHE_MERGE_SUPPORT)
                            device_volumes[2].mount_point = strdup("/.cache");
#else
                            device_volumes[2].mount_point = strdup("/cache");
#endif

#if defined(UBIFS_SUPPORT)
                            if (ubifs_exist("cache")) {
                                device_volumes[2].fs_type = strdup("ubifs");
                            } else {
                                device_volumes[2].fs_type = strdup("yaffs2");
                            }
#else
                            device_volumes[2].fs_type = strdup("yaffs2");
#endif

#if defined(UBIFS_SUPPORT)
                            if (!strcmp(device_volumes[2].fs_type, "yaffs2")) {
                                device_volumes[2].blk_device = strdup("cache");
                            } else {
                                device_volumes[2].blk_device = strdup(dev[CACHE_INDEX]);
                            }
#else
                            device_volumes[2].blk_device = strdup("cache");
#endif
                            device_volumes[2].blk_device2 = NULL;
                            device_volumes[2].length = 0;

                            //data
                            device_volumes[3].mount_point = strdup("/data");

#if defined(UBIFS_SUPPORT)
                            device_volumes[3].fs_type = strdup("ubifs");
#else
                            device_volumes[3].fs_type = strdup("yaffs2");
#endif

#if defined(UBIFS_SUPPORT)
                            //device_volumes[3].blk_device = strdup("/dev/mtd/mtd13");
                            device_volumes[3].blk_device = strdup(dev[DATA_INDEX]);
#else
                            device_volumes[3].blk_device = strdup("userdata");
#endif

                            device_volumes[3].blk_device2 = NULL;
                            device_volumes[3].length = 0;

                            //misc
                            device_volumes[4].mount_point = strdup("/misc");
                            device_volumes[4].fs_type = strdup("mtd");
                            device_volumes[4].blk_device = strdup("misc");
                            device_volumes[4].blk_device2 = NULL;
                            device_volumes[4].length = 0;

                            //recovery
                            device_volumes[5].mount_point = strdup("/recovery");
                            device_volumes[5].fs_type = strdup("mtd");
                            device_volumes[5].blk_device = strdup("recovery");
                            device_volumes[5].blk_device2 = NULL;
                            device_volumes[5].length = 0;

                            //sdcard
                            device_volumes[6].mount_point = strdup("/sdcard");
                            device_volumes[6].fs_type = strdup("vfat");
                            device_volumes[6].blk_device = strdup("/dev/block/mmcblk0p1");
                            device_volumes[6].blk_device2 = strdup("/dev/block/mmcblk0");
                            device_volumes[6].length = 0;

                            //system
                            device_volumes[7].mount_point = strdup("/system");

#if defined(UBIFS_SUPPORT)
                            device_volumes[7].fs_type = strdup("ubifs");
#else
                            device_volumes[7].fs_type = strdup("yaffs2");
#endif

#if defined(UBIFS_SUPPORT)
                            //device_volumes[7].blk_device = strdup("/dev/mtd/mtd11");
                            device_volumes[7].blk_device = strdup(dev[SYSTEM_INDEX]);
#else
                            device_volumes[7].blk_device = strdup("system");
#endif

                            device_volumes[7].blk_device2 = NULL;
                            device_volumes[7].length = 0;

                            if (has_custom) {
                                //custom
                                device_volumes[8].mount_point = strdup("/custom");

#if defined(UBIFS_SUPPORT)
                                device_volumes[8].fs_type = strdup("ubifs");
#else
                                device_volumes[8].fs_type = strdup("yaffs2");
#endif

#if defined(UBIFS_SUPPORT)
                                device_volumes[8].blk_device = strdup(dev[CUSTOM_INDEX]);
#else
                                device_volumes[8].blk_device = strdup("custom");
#endif

                                device_volumes[8].blk_device2 = NULL;
                                device_volumes[8].length = 0;
                            }

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15
                            //NAND no 2nd SD
                            num_volumes--;
#endif //SUPPORT_SDCARD2
                        }

                        mt_ensure_dev_ready(MISC_PART);
                        mt_ensure_dev_ready(CACHE_PART);

                        printf("recovery filesystem table\n");
                        printf("=========================\n");
                        for (j = 0; j < num_volumes; ++j) {
                            Volume* v = &device_volumes[j];
                            printf("  %d %s %s %s %s %lld\n", j, v->mount_point, v->fs_type, v->blk_device, v->blk_device2, v->length);
                        }
                        printf("\n");
                        return;
                    }
                } else {
                    printf("fstab type setting is wrong\n");
                }
            }

        } else {
            printf("Fail to open /proc/dumchar_info\n");
        }
    } // dumchar_info supported
#endif

    FILE* fstab = fopen("/etc/recovery.fstab", "r");
    if (fstab == NULL) {
        LOGE("failed to open /etc/recovery.fstab (%s)\n", strerror(errno));
        return;
    }

    char buffer[1024];
    int i;
    int fstab_has_custom = 0;

    while (fgets(buffer, sizeof(buffer)-1, fstab)) {
        for (i = 0; buffer[i] && isspace(buffer[i]); ++i);
        if (buffer[i] == '\0' || buffer[i] == '#') continue;
        //tonykuo 2013-11-28
        char* original;
        char* device;
        char* mount_point;
        char* fs_type;
        char* options;
        char* device2;

        //tonykuo 2013-11-28
        if (new_fstab) {
            original = strdup(buffer);

            device = strtok(buffer + i, " \t\n");
            mount_point = strtok(NULL, " \t\n");
            fs_type = strtok(NULL, " \t\n");
            char* defualt1 = strtok(NULL, " \t\n");
            char* defualt2 = strtok(NULL, " \t\n");
            // lines may optionally have a second device, to use if
            // mounting the first one fails.
            options = NULL;
            device2 = strtok(NULL, " \t\n");
            if (device2) {
                if (device2[0] == '/') {
                    options = strtok(NULL, " \t\n");
                } else {
                    options = device2;
                    device2 = NULL;
                }
            }
        } else {
            original = strdup(buffer);

            mount_point = strtok(buffer+i, " \t\n");
            fs_type = strtok(NULL, " \t\n");
            device = strtok(NULL, " \t\n");
            // lines may optionally have a second device, to use if
            // mounting the first one fails.
            options = NULL;
            device2 = strtok(NULL, " \t\n");
            if (device2) {
                if (device2[0] == '/') {
                    options = strtok(NULL, " \t\n");
                } else {
                    options = device2;
                    device2 = NULL;
                }
            }
        }


#if 1 //wschen 2013-01-31
        if (mount_point && !strcmp(mount_point, "/custom")) {
            fstab_has_custom = 1;
        }
#endif

        if (mount_point && fs_type && device) {
            while (num_volumes >= alloc) {
                alloc *= 2;
                device_volumes = (Volume*)realloc(device_volumes, alloc*sizeof(Volume));
            }

#if defined(CACHE_MERGE_SUPPORT)
            if (!strcmp(mount_point, "/cache")) {
                mount_point = "/.cache";
            }
#endif
            device_volumes[num_volumes].mount_point = strdup(mount_point);

#if defined(UBIFS_SUPPORT)
            if (!strcmp(mount_point, "/cache") || !strcmp(mount_point, "/.cache")) {
                if (ubifs_exist("cache")) {
                    device_volumes[num_volumes].fs_type = strdup("ubifs");
                } else {
                    device_volumes[num_volumes].fs_type = strdup("yaffs2");
                }
            } else {
                device_volumes[num_volumes].fs_type = strdup(fs_type);
            }
#else
            device_volumes[num_volumes].fs_type = strdup(fs_type);
#endif // not UBIFS_SUPPORT


            if (phone_type == EMMC_TYPE) {
#if defined(CACHE_MERGE_SUPPORT)
                if (!strcmp(mount_point, "/.cache") && strcmp(device, dev[CACHE_INDEX])) {
#else
                if (!strcmp(mount_point, "/cache") && strcmp(device, dev[CACHE_INDEX])) {
#endif
                    device_volumes[num_volumes].blk_device = strdup(dev[CACHE_INDEX]);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else if (!strcmp(mount_point, "/data") && strcmp(device, dev[DATA_INDEX])) {
                    device_volumes[num_volumes].blk_device = strdup(dev[DATA_INDEX]);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else if (!strcmp(mount_point, "/system") && strcmp(device, dev[SYSTEM_INDEX])) {
                    device_volumes[num_volumes].blk_device = strdup(dev[SYSTEM_INDEX]);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else if (!strcmp(mount_point, "/custom") && strcmp(device, dev[CUSTOM_INDEX])) {
                    device_volumes[num_volumes].blk_device = strdup(dev[CUSTOM_INDEX]);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
#if defined(MTK_GMO_ROM_OPTIMIZE)
                } else if (!strcmp(mount_point, "/sdcard")) {
                    device_volumes[num_volumes].blk_device = strdup("/dev/block/mmcblk1p1");
                    device_volumes[num_volumes].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#if defined(MTK_SHARED_SDCARD) || defined(MTK_2SDCARD_SWAP)
                } else if (!strcmp(mount_point, "/sdcard")) {
                    device_volumes[num_volumes].blk_device = strdup("/dev/block/mmcblk1p1");
                    if (has_fat) {
                        device_volumes[num_volumes].blk_device2 = strdup(dev[FAT_INDEX]);
                    } else {
                        device_volumes[num_volumes].blk_device2 = strdup("/dev/block/mmcblk1");
                    }
#else
                } else if (!strcmp(mount_point, "/sdcard")) {
                    if (has_fat) {
                        device_volumes[num_volumes].blk_device = strdup(dev[FAT_INDEX]);
                        device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                    } else {
                        device_volumes[num_volumes].blk_device = strdup("/dev/block/mmcblk1p1");
                        device_volumes[num_volumes].blk_device2 = strdup("/dev/block/mmcblk1");
                    }
#endif // not MTK_SHARED_SDCARD and not MTK_2SDCARD_SWAP
#endif // not MTK_GMO_ROM_OPTIMIZE
                } else {
                    device_volumes[num_volumes].blk_device = strdup(device);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                }

            } else {
                //NAND

                //Tony
#if defined(CACHE_MERGE_SUPPORT)
                if (!strcmp(mount_point, "/.cache") && strcmp(device, dev[CACHE_INDEX])) {
#else
                if (!strcmp(mount_point, "/cache") && strcmp(device, dev[CACHE_INDEX])) {
#endif
#if defined(UBIFS_SUPPORT)
                    if (!strcmp(device_volumes[num_volumes].fs_type, "yaffs2")) {
                        device_volumes[num_volumes].blk_device = strdup("cache");
                    } else {
                        device_volumes[num_volumes].blk_device = strdup(dev[CACHE_INDEX]);
                    }
#else
                    device_volumes[num_volumes].blk_device = strdup("cache");
#endif
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else if (!strcmp(mount_point, "/data") && strcmp(device, dev[DATA_INDEX])) {
#if defined(UBIFS_SUPPORT)
                    device_volumes[num_volumes].blk_device = strdup(dev[DATA_INDEX]);
#else
                    device_volumes[num_volumes].blk_device = strdup("userdata");
#endif
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else if (!strcmp(mount_point, "/system") && strcmp(device, dev[SYSTEM_INDEX])) {
#if defined(UBIFS_SUPPORT)
                    device_volumes[num_volumes].blk_device = strdup(dev[SYSTEM_INDEX]);
#else
                    device_volumes[num_volumes].blk_device = strdup("system");
#endif
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                } else {
                    device_volumes[num_volumes].blk_device = strdup(device);
                    device_volumes[num_volumes].blk_device2 = device2 ? strdup(device2) : NULL;
                }
            }

            device_volumes[num_volumes].length = 0;
            if (parse_options(options, device_volumes + num_volumes) != 0) {
                LOGE("skipping malformed recovery.fstab line: %s\n", original);
            } else {
                ++num_volumes;
            }
        } else {
            LOGE("skipping malformed recovery.fstab line: %s\n", original);
        }
        free(original);
    }

    fclose(fstab);

    if (has_custom && !fstab_has_custom) {
        while (num_volumes >= alloc) {
            alloc *= 2;
            device_volumes = (Volume*)realloc(device_volumes, alloc*sizeof(Volume));
        }
        device_volumes[num_volumes].mount_point = strdup("/custom");

        if (phone_type == EMMC_TYPE) {
            device_volumes[num_volumes].fs_type = strdup("ext4");
            device_volumes[num_volumes].blk_device = strdup(dev[CUSTOM_INDEX]);
        } else {
            device_volumes[num_volumes].fs_type = strdup("yaffs2");
            device_volumes[num_volumes].blk_device = strdup("custom");
        }

        device_volumes[num_volumes].blk_device2 = NULL;
        device_volumes[num_volumes].length = 0;

        num_volumes++;
    }

#if defined(SUPPORT_SDCARD2) && !defined(MTK_SHARED_SDCARD) //wschen 2012-11-15

    if ((phone_type == EMMC_TYPE) && has_fat) {
        int match = 0;
        for (i = 0; i < num_volumes; i++) {
            Volume* v = &device_volumes[i];
            if (strcmp(v->mount_point, "/sdcard2") == 0) {
                match = 1;
                break;
            }
        }

        if (match == 0) {
            while (num_volumes >= alloc) {
                alloc *= 2;
                device_volumes = (Volume*)realloc(device_volumes, alloc*sizeof(Volume));
            }

            device_volumes[num_volumes].mount_point = strdup("/sdcard2");
            device_volumes[num_volumes].fs_type = strdup("vfat");
#if defined(MTK_GMO_ROM_OPTIMIZE)
             device_volumes[num_volumes].blk_device = strdup("/dev/block/mmcblk1p1");
             device_volumes[num_volumes].blk_device2 = strdup("/dev/block/mmcblk1");
#else
#ifdef MTK_2SDCARD_SWAP
            device_volumes[num_volumes].blk_device = strdup(dev[FAT_INDEX]);
            device_volumes[num_volumes].blk_device2 = NULL;
#else
            device_volumes[num_volumes].blk_device = strdup("/dev/block/mmcblk1p1");
            device_volumes[num_volumes].blk_device2 = strdup("/dev/block/mmcblk1");
#endif
#endif

            device_volumes[num_volumes].length = 0;
            num_volumes++;
        }
    }

#endif //SUPPORT_SDCARD2

    mt_ensure_dev_ready(MISC_PART);
    mt_ensure_dev_ready(CACHE_PART);

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = &device_volumes[i];
        printf("  %d %s %s %s %s %lld\n", i, v->mount_point, v->fs_type,
               v->blk_device, v->blk_device2, v->length);
    }
    printf("\n");
}

