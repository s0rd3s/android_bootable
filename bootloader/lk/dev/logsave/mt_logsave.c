#ifdef MTK_LOG_SAVE_SUPPORT
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <kernel/thread.h>
#include <platform/ram_console.h>

#define LOG_SAVE_SIG    0x88A8
#define LOG_SAVE_ADDR   (RAM_CONSOLE_SRAM_ADDR + 512)
#define LOG_SIZE        (3 << 10)       // 3KB

struct profile_buffer_s {
    uint32_t sig;
    char lprofile[LOG_SIZE + 1];
    char profile[LOG_SIZE + 1];
    unsigned int pos;
};

static struct profile_buffer_s *buffer = NULL;

void mt_log_save_init(void);
const char *mt_log_save_llog_get(void);
void mt_log_save_put_char(char);

void mt_log_save_init(void)
{
    enter_critical_section();

    buffer = (struct profile_buffer_s *) LOG_SAVE_ADDR;

    if (buffer->sig != LOG_SAVE_SIG) {
        buffer->sig = LOG_SAVE_SIG;
        memset(&buffer->lprofile, 0x0, sizeof(buffer->lprofile));
    } else {
        memcpy(&buffer->lprofile, &buffer->profile, sizeof(buffer->lprofile));
    }
    memset(&buffer->profile, 0x0, sizeof(buffer->profile));
    buffer->pos = 0;

    exit_critical_section();
}

const char *mt_log_save_llog_get(void)
{
    buffer->lprofile[LOG_SIZE] = '\0';

    return (const char *) &buffer->lprofile;
}

void mt_log_save_put_char(char c)
{
    if (c != '\0' && buffer) {
        enter_critical_section();
        buffer->profile[buffer->pos++] = c;
        if (buffer->pos == LOG_SIZE - 1)
            buffer->pos = 0;
        exit_critical_section();
    }
}
#endif
