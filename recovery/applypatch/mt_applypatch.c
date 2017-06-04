/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#include <stdio.h>
#include "applypatch.h"
#include "mt_applypatch.h"

int phone_type(void)
{

    FILE *dum;
    char buf[512];
    char p_name[32], p_size[32], p_addr[32], p_actname[64];
    unsigned int p_type;
    //0 -> NAND; 1 -> EMMC
    int phone_type = -1;

    dum = fopen("/proc/dumchar_info", "r");
    if (dum) {
        if (fgets(buf, sizeof(buf), dum) != NULL) {
            while (fgets(buf, sizeof(buf), dum)) {
                if (sscanf(buf, "%s %s %s %d %s", p_name, p_size, p_addr, &p_type, p_actname) == 5) {
                    if (!strcmp(p_name, "bmtpool")) {
                        break;
                    }
                    if (!strcmp(p_name, "preloader")) {
                        if (p_type == 2) {
                            phone_type = EMMC_TYPE;
                            break;
                        } else {
                            phone_type = NAND_TYPE;
                            break;
                        }
                    }
                }
            }
        }
        fclose(dum);
        return phone_type;
    } else {
        return -1;
    }
}


//TEE update related
int LoadTeeContents(const char* filename, FileContents* file) {
    file->data = NULL;

    if (stat(filename, &file->st) != 0) {
        printf("failed to stat \"%s\": %s\n", filename, strerror(errno));
        return -1;
    }

    file->size = file->st.st_size;
    file->data = malloc(file->size);

    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        printf("failed to open \"%s\": %s\n", filename, strerror(errno));
        free(file->data);
        file->data = NULL;
        return -1;
    }

    ssize_t bytes_read = fread(file->data, 1, file->size, f);
    if (bytes_read != file->size) {
        printf("short read of \"%s\" (%ld bytes of %ld)\n",
               filename, (long)bytes_read, (long)file->size);
        free(file->data);
        file->data = NULL;
        return -1;
    }
    fclose(f);

    return 0;
}

int TeeUpdate(const char* tee_image, const char* target_filename) {

    FileContents source_file;
    source_file.data = NULL;

    if (LoadTeeContents(tee_image, &source_file) == 0) {
        if (WriteToPartition(source_file.data, source_file.size, target_filename) != 0) {
            printf("write of patched data to %s failed\n", target_filename);
            free(source_file.data);
            return 1;
        }
    }

    free(source_file.data);
    // Success!
    return 0;
}
