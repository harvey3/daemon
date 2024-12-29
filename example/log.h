/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_VERSION "0.1.0"

typedef int (*log_LockFn)(void *udata);
typedef int (*log_UnLockFn)(void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#define TBUF_LEN 2048
#define MAX_FILE_SIZE 8000000
#define MAXTRIES 5
#define PATH_LOG "/var/log/daemon_logger"
#define PATH_HEARTBEAT "/var/log/daemon_heartbeat"
    

#define LOG_DATA_INIT { \
    .ident = NULL, \
    .log_file = -1, \
    .log_connected = 0, \
    .log_opened = 0, \
    .level = 0, \
}
        
#define MAX_THREAD_PER_PROCESS 10
    
#ifdef __cplusplus
extern "C" 
{
#endif    
void log_open(const char *ident, int prio);
void log_close();
void log_init(char *path, int prio);
void log_log(int level, const char *file, int line, const char *fmt, ...);
void heartbeat_init(void);
void heartbeat_timeout(int sec);
#ifdef __cplusplus
}

#endif    
#endif
