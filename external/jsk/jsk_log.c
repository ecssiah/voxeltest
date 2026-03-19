#include "jsk_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static const char* LOG_LEVEL_TO_STRING[LOG_LEVEL_COUNT] =
{
    "TRACE",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

static FILE* LOG_FILE;
static char LOG_BASE_PATH[256];
static char CURRENT_DAY_CHAR[11];

void log_init()
{
    const char* directory_name = "log/";

    strncpy(LOG_BASE_PATH, directory_name, sizeof(LOG_BASE_PATH) - 1);

    if (!LOG_FILE)
    {
        LOG_FILE = stderr;
    }

    LOG_INFO("\n\nLOGGING INIT\n");
}

void log_message(ELogLevel log_level, const char* file, int line, const char* fmt, ...)
{
    if (!LOG_FILE)
    {
        LOG_FILE = stderr;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char file_timestamp[11];
    strftime(file_timestamp, sizeof(file_timestamp), "%Y_%m_%d", &tm_info);

    if (LOG_BASE_PATH[0] != '\0' && strcmp(file_timestamp, CURRENT_DAY_CHAR) != 0)
    {
        if (LOG_FILE && LOG_FILE != stderr)
        {
            fclose(LOG_FILE);
        }

        strncpy(CURRENT_DAY_CHAR, file_timestamp, sizeof(CURRENT_DAY_CHAR) - 1);

        char path[512];
        snprintf(path, sizeof(path), "%sengine_%s.log", LOG_BASE_PATH, file_timestamp);

        LOG_FILE = fopen(path, "a");
        if (!LOG_FILE)
        {
            LOG_FILE = stderr;
        }
    }

    const char* filename = file;
    const char* last_slash = strrchr(file, '/');

    if (last_slash) 
    {
        filename = last_slash + 1;
    }

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(
        stderr, 
        "[%s] [%s] (%s:%d) ",
        timestamp,
        LOG_LEVEL_TO_STRING[log_level],
        filename,
        line
    );

    if (LOG_FILE && LOG_FILE != stderr) 
    {
        fprintf(
            LOG_FILE, 
            "[%s] [%s] (%s:%d) ",
            timestamp,
            LOG_LEVEL_TO_STRING[log_level],
            filename,
            line
        );
    }

    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);

    vfprintf(stderr, fmt, args);

    if (LOG_FILE && LOG_FILE != stderr)
    {
        vfprintf(LOG_FILE, fmt, args_copy);
    }

    va_end(args_copy);
    va_end(args);

    fprintf(stderr, "\n");

    if (LOG_FILE && LOG_FILE != stderr) 
    {
        fprintf(LOG_FILE, "\n");
        fflush(LOG_FILE);
    }

    if (log_level == LOG_LEVEL_FATAL)
    {
        fflush(stderr);

        if (LOG_FILE && LOG_FILE != stderr) 
        {
            fflush(LOG_FILE);
        }

        exit(EXIT_FAILURE);
    }
}

void log_destroy()
{
    LOG_INFO("\n\nLOGGING SHUTDOWN\n");

    if (LOG_FILE && LOG_FILE != stderr)
    {
        fclose(LOG_FILE);
    }
}
