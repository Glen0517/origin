#include "storage_priv.h"
#include "logger.h"

static int g_file_reader_init = 0;

int file_reader_init(void)
{
    g_file_reader_init = 1;
    LOG_INFO("Storage: Local file reader init (MP3/WAV support)");
    return 0;
}

void file_reader_deinit(void)
{
    g_file_reader_init = 0;
}