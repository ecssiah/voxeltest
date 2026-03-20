#include "jsk_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static FILE* JSK_LOG_FILE;
static char JSK_LOG_BASE_PATH[256];
static char JSK_CURRENT_DAY_STRING[11];

static const char* LOG_LEVEL_TO_STRING[LOG_LEVEL_COUNT] =
{
    "TRACE",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};


void log_init()
{
    const char* directory_name = "log/";

    strncpy(JSK_LOG_BASE_PATH, directory_name, sizeof(JSK_LOG_BASE_PATH) - 1);

    if (!JSK_LOG_FILE)
    {
        JSK_LOG_FILE = stderr;
    }

    LOG_INFO("\n\nLOGGING INIT\n");
}

void log_message(ELogLevel log_level, const char* file, int line, const char* fmt, ...)
{
    if (!JSK_LOG_FILE)
    {
        JSK_LOG_FILE = stderr;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char file_timestamp[11];
    strftime(file_timestamp, sizeof(file_timestamp), "%Y_%m_%d", &tm_info);

    if (JSK_LOG_BASE_PATH[0] != '\0' && strcmp(file_timestamp, JSK_CURRENT_DAY_STRING) != 0)
    {
        if (JSK_LOG_FILE && JSK_LOG_FILE != stderr)
        {
            fclose(JSK_LOG_FILE);
        }

        strncpy(JSK_CURRENT_DAY_STRING, file_timestamp, sizeof(JSK_CURRENT_DAY_STRING) - 1);

        char path[512];
        snprintf(path, sizeof(path), "%sengine_%s.log", JSK_LOG_BASE_PATH, file_timestamp);

        JSK_LOG_FILE = fopen(path, "a");
        if (!JSK_LOG_FILE)
        {
            JSK_LOG_FILE = stderr;
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

    if (JSK_LOG_FILE && JSK_LOG_FILE != stderr) 
    {
        fprintf(
            JSK_LOG_FILE, 
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

    if (JSK_LOG_FILE && JSK_LOG_FILE != stderr)
    {
        vfprintf(JSK_LOG_FILE, fmt, args_copy);
    }

    va_end(args_copy);
    va_end(args);

    fprintf(stderr, "\n");

    if (JSK_LOG_FILE && JSK_LOG_FILE != stderr) 
    {
        fprintf(JSK_LOG_FILE, "\n");
        fflush(JSK_LOG_FILE);
    }

    if (log_level == LOG_LEVEL_FATAL)
    {
        fflush(stderr);

        if (JSK_LOG_FILE && JSK_LOG_FILE != stderr) 
        {
            fflush(JSK_LOG_FILE);
        }

        exit(EXIT_FAILURE);
    }
}

void log_destroy()
{
    LOG_INFO("\n\nLOGGING SHUTDOWN\n");

    if (JSK_LOG_FILE && JSK_LOG_FILE != stderr)
    {
        fclose(JSK_LOG_FILE);
    }
}
