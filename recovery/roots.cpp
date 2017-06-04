/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#if defined(HAVE_ANDROID_OS) && !defined(ARCH_X86) //wschen 2012-07-10
#include <linux/mmc/sd_misc.h>
#endif
#include <time.h>
#include "mt_roots.h"
#include "mt_roots_ubi.h"
//#include <fs_mgr.h>  //tonykuo 2013-12-05
#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"
#include "common.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "make_ext4fs.h"
#ifdef __cplusplus
}
#endif

extern "C" {
#include "wipe.h"
#ifdef USE_EXT4
#include "cryptfs.h"
#endif
}

//static struct fstab *fstab = NULL;

extern struct selabel_handle *sehandle;

static const char* PERSISTENT_PATH = "/persistent";

#if defined(CACHE_MERGE_SUPPORT)
#include <dirent.h>
#include "mt_check_partition.h"

static int need_clear_cache = 0;
static const char *DATA_CACHE_ROOT = "/data/.cache";
#endif

void load_volume_table()
{
#if 0
    int i;
    int ret;

    fstab = fs_mgr_read_fstab("/etc/recovery.fstab");
    if (!fstab) {
        LOGE("failed to read /etc/recovery.fstab\n");
        return;
    }

    ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk");
    if (ret < 0 ) {
        LOGE("failed to add /tmp entry to fstab\n");
        fs_mgr_free_fstab(fstab);
        fstab = NULL;
        return;
    }

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        printf("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
               v->blk_device, v->length);
    }
    printf("\n");
#endif
    mt_load_volume_table();
}

Volume* volume_for_path(const char* path) {
#if 0
  return fs_mgr_get_entry_for_mount_point(fstab, path);
#endif
  return mt_volume_for_path(path);
}

int ensure_path_mounted(const char* path) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv) {
#if defined(CACHE_MERGE_SUPPORT)
        if (strncmp(path, "/cache", 6) == 0) {
            if (symlink(DATA_CACHE_ROOT, "/cache")) {
                if (errno != EEXIST) {
                    LOGE("create symlink from %s to %s failed(%s)\n",
                            DATA_CACHE_ROOT, "/cache", strerror(errno));
                    return -1;
                }
            }
        }
#endif
        // volume is already mounted
        return 0;
    }

    mkdir(v->mount_point, 0755);  // in case it doesn't already exist

#if defined (UBIFS_SUPPORT)
    if (strcmp(v->fs_type, "ubifs") == 0) {

        printf("Trying to mount %s \n", v->mount_point);

        //Attatch UBI device & Make UBI volum
        int n = -1;
        n = ubi_attach_mtd_user(v->mount_point);

        if ((n != -1) && (n < 4)) {
            printf("Try to attatch %s \n", v->blk_device);
            printf("/dev/ubi%d_0 is attached \n", n);
        } else {
            LOGE("failed to attach %s\n", v->blk_device);
        }


        //Mount UBI volume
        const unsigned long flags = MS_NOATIME | MS_NODEV | MS_NODIRATIME;
        char tmp[64];
        sprintf(tmp, "/dev/ubi%d_0", n);
        wait_for_file(tmp, 5);
        result = mount(tmp, v->mount_point, v->fs_type, flags, "");
        if (result < 0) {
            ubi_detach_dev(n);
            return -1;
        } else if (result == 0) {
            goto mount_done;  //Volume  successfully  mounted
        }

    }
#endif

    if (strcmp(v->fs_type, "yaffs2") == 0) {
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("failed to find \"%s\" partition to mount at \"%s\"\n",
                    v->blk_device, v->mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, v->mount_point, v->fs_type, 0);
    } else if (strcmp(v->fs_type, "ext4") == 0 ||
            strcmp(v->fs_type, "vfat") == 0) {
        result = mount(v->blk_device, v->mount_point, v->fs_type,
                MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if (result == 0) {
            goto mount_done;
        } else {
#if 1 //wschen 2013-05-03 workaround for slowly SD
            if (strstr(v->mount_point, "/sdcard") && (strstr(v->blk_device, "/dev/block/mmcblk1") || strstr(v->blk_device, "/dev/block/mmcblk0"))) {
                int retry = 0;
                for (; retry <= 3; retry++) {
                    result = mount(v->blk_device, v->mount_point, v->fs_type, MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
                    if (result == 0) {
                        goto mount_done;
                    } else {
                        sleep(1);
                    }
                }
                //tonykuo 2014-04-11 Try mount mmcblk0 in case mmcblk0p1 failed
                if (strstr(v->blk_device, "/dev/block/mmcblk0")) {
                    result = mount("/dev/block/mmcblk0", v->mount_point, v->fs_type, MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
                    if (result == 0) {
                        goto mount_done;
                    }
                }

                printf("Slowly SD retry failed (%s)\n", v->blk_device);
            }
#endif
        }

        if (v->blk_device2) {
#if 1 //wschen 2012-09-04
            //try mmcblk1 if mmcblk1p1 failed, then try internal FAT
            if (!strcmp(v->mount_point, "/sdcard") && !strcmp(v->blk_device, "/dev/block/mmcblk1p1") && !strstr(v->blk_device2, "/dev/block/mmcblk1")) {
                result = mount("/dev/block/mmcblk1", v->mount_point, v->fs_type, MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
                if (result == 0) {
                    return 0;
                }
            }
#endif
            LOGW("failed to mount %s (%s); trying %s\n",
                    v->blk_device, strerror(errno), v->blk_device2);
            result = mount(v->blk_device2, v->mount_point, v->fs_type,
                    MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if (result == 0) goto mount_done;
        }

        LOGE("failed to mount %s (%s)\n", v->mount_point, strerror(errno));
        return -1;
    }

    LOGE("unknown fs_type \"%s\" for %s\n", v->fs_type, v->mount_point);
    return -1;

mount_done:
#if defined(CACHE_MERGE_SUPPORT)
    if (strcmp(v->mount_point, "/data") == 0) {
        if (mkdir(DATA_CACHE_ROOT, 0770)) {
            if (errno != EEXIST) {
                LOGE("mkdir %s error: %s\n", DATA_CACHE_ROOT, strerror(errno));
                return -1;
            } else if (need_clear_cache) {
                LOGI("cache exists, clear it...\n");
                if (remove_dir(DATA_CACHE_ROOT)) {
                    LOGE("remove_dir %s error: %s\n", DATA_CACHE_ROOT, strerror(errno));
                    return -1;
                }
                if (mkdir(DATA_CACHE_ROOT, 0770) != 0) {
                    LOGE("mkdir %s error: %s\n", DATA_CACHE_ROOT, strerror(errno));
                    return -1;
                }
            }
        }
        if (symlink(DATA_CACHE_ROOT, "/cache")) {
            if (errno != EEXIST) {
                LOGE("create symlink from %s to %s failed(%s)\n",
                        DATA_CACHE_ROOT, "/cache", strerror(errno));
                return -1;
            }
        }
        need_clear_cache = 0;
    }
#endif
    return 0;
}

int ensure_path_unmounted(const char* path) {
    Volume* v = volume_for_path(path);

#if defined(CACHE_MERGE_SUPPORT)
    if (strncmp(path, "/cache", 6) == 0) {
        unlink(path);
        return 0;
    }
#endif

    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    return unmount_mounted_volume(mv);
}

static int exec_cmd(const char* path, char* const argv[]) {
    int status;
    pid_t child;
    if ((child = vfork()) == 0) {
        execv(path, argv);
        _exit(-1);
    }
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGE("%s failed with status %d\n", path, WEXITSTATUS(status));
    }
    return WEXITSTATUS(status);
}

int format_volume(const char* volume) {

    time_t start, end;
    start = time((time_t *)NULL);
    printf("format %s start=%u\n", volume, (unsigned int)start);

#if defined(CACHE_MERGE_SUPPORT)
    char *target_volume = (char *)volume;

    if (strcmp(target_volume, "/cache") == 0) {
        // we cannot mount data since partition size changed
        // clear cache folder when data mounted
        if (part_size_changed) {
            LOGI("partition size changed, clear cache folder when data mounted...\n");
            need_clear_cache = 1;

            // change format volume name to format actual cache partition
            target_volume = "/.cache";
        } else {
            // clear DATA_CACHE_ROOT
            if (ensure_path_mounted(DATA_CACHE_ROOT) != 0) {
                LOGE("Can't mount %s while clearing cache!\n", DATA_CACHE_ROOT);
                return -1;
            }
            if (remove_dir(DATA_CACHE_ROOT)) {
                LOGE("remove_dir %s error: %s\n", DATA_CACHE_ROOT, strerror(errno));
                return -1;
            }
            if (mkdir(DATA_CACHE_ROOT, 0770) != 0) {
                LOGE("Can't mkdir %s (%s)\n", DATA_CACHE_ROOT, strerror(errno));
                return -1;
            }
            LOGI("format cache successfully!\n");

            end = time((time_t *)NULL);
            printf("format end=%u duration=%u\n", (unsigned int)end, (unsigned int)(end - start));
            return 0;
        }
    }

    Volume* v = volume_for_path(target_volume);
    if (v == NULL) {
        LOGE("unknown volume \"%s\"\n", target_volume);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", target_volume);
        return -1;
    }
    if (strcmp(v->mount_point, target_volume) != 0) {
        LOGE("can't give path \"%s\" to format_volume\n", target_volume);
        return -1;
    }

    if (ensure_path_unmounted(target_volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }
#else
    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        LOGE("unknown volume \"%s\"\n", volume);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", volume);
        return -1;
    }
    if (strcmp(v->mount_point, volume) != 0) {
        LOGE("can't give path \"%s\" to format_volume\n", volume);
        return -1;
    }

    if (ensure_path_unmounted(volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }
#endif

#if defined (UBIFS_SUPPORT)
    if (strcmp(v->fs_type, "ubifs") == 0) {

        int ret;
        ret = ubi_format(v->mount_point);

        if (!ret) {
            end = time((time_t *)NULL);
            printf("format end=%u duration=%u\n", (unsigned int)end, (unsigned int)(end - start));
            return 0;
        } else {
            LOGE("Ubiformat failed on \"%s\"\n", v->mount_point);
        }


#if 0

        int ret;
        //Remove volume
        if(ubi_rmvol_user(v->mount_point)!=0){
            LOGE("failed to remove %s\n", v->blk_device);
            return -1;
        }

        //Make volume
        ret = ubi_mkvol_user(v->mount_point);
        if(!ret){
            printf("%s volume made\n", v->blk_device);
            return 0;
        }
#endif
    }
#endif

    if (strcmp(v->fs_type, "yaffs2") == 0 || strcmp(v->fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", v->blk_device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", v->blk_device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", v->blk_device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n", v->blk_device);
            return -1;
        }

        end = time((time_t *)NULL);
        printf("format end=%u duration=%u\n", (unsigned int)end, (unsigned int)(end - start));
        return 0;
    }

    if (strcmp(v->fs_type, "ext4") == 0 || strcmp(v->fs_type, "f2fs") == 0) {
        if (!is_support_gpt()) {
#if defined(HAVE_ANDROID_OS) && !defined(ARCH_X86) //wschen 2012-07-10
            int fd;
            struct msdc_ioctl msdc_io;

            fd = open("/dev/misc-sd", O_RDONLY);
            if (fd < 0) {
                LOGE("open: /dev/misc-sd failed\n");
                return -1;
            }

            msdc_io.opcode = MSDC_ERASE_PARTITION;
#if defined(CACHE_MERGE_SUPPORT)
            if (!strcmp(target_volume, "/.cache")) {
                msdc_io.buffer = (unsigned int*) "cache";
                msdc_io.total_size = 6;
            } else if (!strcmp(target_volume, "/data")) {
                msdc_io.buffer = (unsigned int*) "usrdata";
                msdc_io.total_size = 8;
            }
#else
            if (!strcmp(volume, "/cache")) {
                msdc_io.buffer = (unsigned int*) "cache";
                msdc_io.total_size = 6;
            } else if (!strcmp(volume, "/data")) {
                msdc_io.buffer = (unsigned int*) "usrdata";
                msdc_io.total_size = 8;
            }
#endif
            ioctl(fd, 0, &msdc_io);
            close(fd);
#endif
        } // end of not support_gpt

#if 0   // Currently no support
        // if there's a key_loc that looks like a path, it should be a
        // block device for storing encryption metadata.  wipe it too.
        if (v->key_loc != NULL && v->key_loc[0] == '/') {
            LOGI("wiping %s\n", v->key_loc);
            int fd = open(v->key_loc, O_WRONLY | O_CREAT, 0644);
            if (fd < 0) {
                LOGE("format_volume: failed to open %s\n", v->key_loc);
                return -1;
            }
            wipe_block_device(fd, get_file_size(fd));
            close(fd);
        }

        ssize_t length = 0;
        if (v->length != 0) {
            length = v->length;
        } else if (v->key_loc != NULL && strcmp(v->key_loc, "footer") == 0) {
            length = -CRYPT_FOOTER_OFFSET;
#endif

        int result = 0;
        if (strcmp(v->fs_type, "ext4") == 0) {

#if defined(CACHE_MERGE_SUPPORT)
            result = make_ext4fs(v->blk_device, v->length, target_volume, sehandle);
#else
            LOGE("Before make_ext4fs v->blk_device:%s v->length:%lld volume=%s\n", v->blk_device, v->length, volume);
            result = make_ext4fs(v->blk_device, v->length, volume, sehandle);
#endif

        } else {   /* Has to be f2fs because we checked earlier. */
#if 0 //currently no support
            if (v->key_loc != NULL && strcmp(v->key_loc, "footer") == 0 && length < 0) {
                LOGE("format_volume: crypt footer + negative length (%zd) not supported on %s\n", length, v->fs_type);
                return -1;
            }
            if (length < 0) {
                LOGE("format_volume: negative length (%zd) not supported on %s\n", length, v->fs_type);
                return -1;
            }
            char *num_sectors;
            if (asprintf(&num_sectors, "%zd", length / 512) <= 0) {
                LOGE("format_volume: failed to create %s command for %s\n", v->fs_type, v->blk_device);
                return -1;
            }
            const char *f2fs_path = "/sbin/mkfs.f2fs";
            const char* const f2fs_argv[] = {"mkfs.f2fs", "-t", "-d1", v->blk_device, num_sectors, NULL};

            result = exec_cmd(f2fs_path, (char* const*)f2fs_argv);
            free(num_sectors);
#endif
        }

        if (result != 0) {
            LOGE("format_volume: make %s failed on %s with %d(%s)\n", v->fs_type, v->blk_device, result, strerror(errno));
            return -1;
        }

        end = time((time_t *)NULL);
        printf("format end=%u duration=%u\n", (unsigned int)end, (unsigned int)(end - start));
        return 0;
    }

    LOGE("format_volume: fs_type \"%s\" unsupported\n", v->fs_type);
    return -1;
}

int erase_persistent_partition() {
    Volume *v = volume_for_path(PERSISTENT_PATH);
    if (v == NULL) {
        // most devices won't have /persistent, so this is not an error.
        return 0;
    }

    int fd = open(v->blk_device, O_RDWR);
    uint64_t size = get_file_size(fd);
    if (size == 0) {
        LOGE("failed to stat size of /persistent\n");
        close(fd);
        return -1;
    }

    char oem_unlock_enabled;
    lseek(fd, size - 1, SEEK_SET);
    read(fd, &oem_unlock_enabled, 1);

    if (oem_unlock_enabled) {
        if (wipe_block_device(fd, size)) {
           LOGE("error wiping /persistent: %s\n", strerror(errno));
           close(fd);
           return -1;
        }

        lseek(fd, size - 1, SEEK_SET);
        write(fd, &oem_unlock_enabled, 1);
    }

    close(fd);

    return (int) oem_unlock_enabled;
}

int setup_install_mounts() {
#if 0
    if (fstab == NULL) {
        LOGE("can't set up install mounts: no fstab loaded\n");
        return -1;
    }
    for (int i = 0; i < fstab->num_entries; ++i) {
        Volume* v = fstab->recs + i;

        if (strcmp(v->mount_point, "/tmp") == 0 ||
            strcmp(v->mount_point, "/cache") == 0) {
            if (ensure_path_mounted(v->mount_point) != 0) {
                LOGE("failed to mount %s\n", v->mount_point);
                return -1;
            }

        } else {
            if (ensure_path_unmounted(v->mount_point) != 0) {
                LOGE("failed to unmount %s\n", v->mount_point);
                return -1;
            }
        }
    }
    return 0;
#endif
  return mt_setup_install_mounts();
}

