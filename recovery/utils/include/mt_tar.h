/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
#ifndef MT_TAR_H_
#define MT_TAR_H_

#include <stdlib.h>
#include <setjmp.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <selinux/selinux.h>

int tar_main(int mode, char *name, char *dirpath);
int setjmp_env(void);
void set_child_pid(int pid);
int get_child_pid(void);
int get_total_files(void);
void reset_total_files(void);
void add_total_files(void);
void set_total_files(int set_num);
void reset_count_files(void);
void check_my_nextFile(void);

#define LONE_CHAR(s,c)     ((s)[0] == (c) && !(s)[1])
#define DOT_OR_DOTDOT(s)   ((s)[0] == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))

#define MY_TAR_BLOCK_SIZE 0x200
#define MY_NAME_SIZE      0x64

#define piped_pair(pair)  pipe(&((pair).rd))
#define xpiped_pair(pair) xpipe(&((pair).rd))

typedef struct llist_t {
    char *data;
    struct llist_t *link;
} llist_t;



typedef struct file_header_t {
    char *name;
    char *link_target;
    off_t size;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    time_t mtime;
    dev_t device;
} file_header_t;

typedef struct archive_handle_t {
    int src_fd;

    char (*filter)(struct archive_handle_t *);
    llist_t *accept;
    llist_t *reject;
    llist_t *passed;

    file_header_t *file_header;

    void (*action_header)(const file_header_t *);

    void (*action_data)(struct archive_handle_t *);

    void (*seek)(int fd, off_t amount);

    off_t offset;

#define PAX_NEXT_FILE 0
#define PAX_GLOBAL 1

    int tar__end;
    char *mtar_longn;
    char *mtar_linkn;
    char *mtar_sctx[2];
} archive_handle_t;


typedef struct my_tar_header_t {
    char name[MY_NAME_SIZE];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[MY_NAME_SIZE];
    char magic[8];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[0x9b];
    char padding[12];
} my_tar_header_t;

typedef struct my_HardLinkInfo {
    struct my_HardLinkInfo *next;
    dev_t dev;
    ino_t ino;
    char name[1];
} my_HardLinkInfo;


typedef struct my_TBallInfo {
    int tarFd;
    my_HardLinkInfo *hlInfoHead;
    my_HardLinkInfo *hlInfo;
    struct stat tarFileStatBuf;
} my_TBallInfo;


struct fd_pair { int rd; int wr; };

#endif
