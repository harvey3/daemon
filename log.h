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

#define MAX_FILE_SIZE 1000000

#define QT_LAC_LOG_PATH            "/usr/longertek/log/qt_lac.log"
#define AIRCON_MAIN_LOG_PATH       "/usr/longertek/log/aircon_main.log"
#define AIRCON_A_LOG_PATH          "/usr/longertek/log/aircon_a.log"
#define AIRCON_B_LOG_PATH          "/usr/longertek/log/aircon_b.log"
#define AIRCON_DRIVER_LOG_PATH     "/usr/longertek/log/aircon_driver.log"

void log_init(char *path, int prio);
void log_set_udata(void *udata);
void log_set_lock(log_LockFn lockFn, log_UnLockFn unlockFn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_log(int level, const char *file, int line, const char *fmt, ...);
void copy_log(char *log, int len);

#endif
