/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#include "mt_common_tk.h"
#include "mt_tar.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

int open_or_warn(const char *pathname, int flags)
{
    int ret;

    ret = open(pathname, flags, 0666);

    if (ret < 0) {
        ui->Print("Error: can't open '%s'\n", pathname);
    }
    return ret;
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
    ssize_t n;

    do {
        n = read(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}

void *mtk_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0) {
        ui->Print("Error: function:%s line:%d Out of memory\n",__FUNCTION__,__LINE__);
    }
    return ptr;
}

void *xzalloc(size_t size)
{
    void *ptr = mtk_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void close_fd(char *pattern) {
    DIR *dir;
    int fd;
    int len;
    char buf[512];
    char path[128];
    struct dirent *ptr;

    dir = opendir("/proc/self/fd");
    if (dir) {
        while ((ptr = readdir(dir)) != NULL) {
            snprintf(path, sizeof(path), "/proc/self/fd/%s", ptr->d_name);
            len = readlink(path, buf, 512);
            if (len != -1) {
                buf[len] = '\0';
                if (strstr(buf, pattern) == buf) {
                    close(atoi(ptr->d_name));
                }
            }
        }
        closedir(dir);
    }
}

void walker(const char *dir)
{
    struct dirent *entry;
    DIR *d;
    struct stat fs;
    char pd[MAXPD];

    if (!(d = opendir(dir))) {
        return;
    }

    while ((entry = readdir(d))) {
        if ((strcmp(".", entry->d_name) == 0) || (strcmp("..", entry->d_name) == 0)) {
            continue;
        }

        if (stat(entry->d_name, &fs) < 0) {
            closedir(d);
            return;
        }

        add_total_files();

        if (S_ISDIR(fs.st_mode)) {
            if (getcwd(pd, MAXPD) == NULL) {
                closedir(d);
                return;
            }
            if (chdir(entry->d_name) < 0) {
                closedir(d);
                return;
            }
            walker(".");
            if (chdir(pd) < 0) {
                closedir(d);
                return;
            }
        }
    }
    closedir(d);
}
