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

#define MAX_FILE_SIZE 8000000

#define MAX_THREAD_PER_PROCESS 10

struct logger_data 
{
    FILE *fd;
    int sd;
    int pos;
    char *log_path;
    
};

struct heartbeat_item 
{
    int pid;
    time_t next_time;
    time_t cycle;
    
};

struct heartbeat_data 
{
    int sd;
    struct heartbeat_item items[MAX_THREAD_PER_PROCESS * 5];
    int item_cnt;
   

};
    
void logger_init(char *path);
void logger_loop(struct logger_data *data);

void heartbeat_init(void);
void heartbeat_loop(struct heartbeat_data *data);

void log_init(char *path, int prio);
void log_set_udata(void *udata);
void log_set_lock(log_LockFn lockFn, log_UnLockFn unlockFn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_log(int level, const char *file, int line, const char *fmt, ...);
void copy_log(char *log, int len);

#endif
