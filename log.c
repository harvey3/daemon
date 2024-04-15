/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "log.h"

static struct {
  void *udata;
  log_LockFn lock;
  log_UnLockFn unlock;
  FILE *fp;
  int level;
  unsigned int pos;
} L;

static pthread_mutex_t log_mutex;


static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void log_init(char *path, int prio)
{
    FILE *fd;
    int fd_no;
    
    memset(&L, 0, sizeof(L));
    log_set_level(prio);
    if (pthread_mutex_init(&log_mutex, NULL)) {
	log_error("open log file fail");
	return;
    }
    
    log_set_udata(&log_mutex);
    log_set_lock(pthread_mutex_lock, pthread_mutex_unlock);

    errno = 0;
    fd = fopen(path, "rt+");
    if (!fd) {
        if (errno != ENOENT)
            return;
        
	fd = fopen(path, "w+");
	if (!fd) {
	    return;
        }
	else {

        L.pos += fprintf(fd, "log offset:23        ");
	    L.pos += fprintf(fd, "\n");
	    fflush(fd);
        fd_no = fileno(fd);
        fsync(fd_no);


	}

    }
    else {
        fseek(fd, 11, SEEK_SET);
        fscanf(fd, "%u", &L.pos);
    }
    log_set_fp(fd);

}

void log_set_udata(void *udata) {
  L.udata = udata;
}

void log_set_lock(log_LockFn lockFn, log_UnLockFn unlockFn) {
  L.lock = lockFn;
  L.unlock = unlockFn;
  
}

void log_set_fp(FILE *fp) {
  L.fp = fp;
}


void log_set_level(int level) {
  L.level = level;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
  if (level < L.level) {
    return;
  }

  /* Acquire lock */

  if (L.lock) {
    L.lock(L.udata);
  }

  /* Get current time */
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);

  /* Log to file */
  if (L.fp) {
    va_list args;
    char buf[32];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    if (L.pos > MAX_FILE_SIZE) {
    	L.pos = 22;
    	fseek(L.fp, 22, SEEK_SET);

    } else
    	fseek(L.fp, L.pos, SEEK_SET);

    L.pos += fprintf(L.fp, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
    va_start(args, fmt);
    L.pos += vfprintf(L.fp, fmt, args);
    va_end(args);
    L.pos += fprintf(L.fp, "\n");
    /* rewind(L.fp); */
    fseek(L.fp, 11, SEEK_SET);
    fprintf(L.fp, "%10u", L.pos);
    fflush(L.fp);
  }

  /* Release lock */
  if (L.unlock) {
    L.unlock(L.udata);
  }

}

void copy_log(char *log, int len) {

    int fd;
    
  /* Acquire lock */

  if (L.lock) {
    L.lock(L.udata);
  }


  /* Log to file */
  if (L.fp) {

    if (L.pos > MAX_FILE_SIZE) {
    	L.pos = 22;
    	fseek(L.fp, 22, SEEK_SET);

    } else
    	fseek(L.fp, L.pos, SEEK_SET);

    /* L.pos += fprintf(L.fp, "%s", log); */
    fwrite(log, len, 1, L.fp);
    L.pos += len;
    L.pos += fprintf(L.fp, "\n");
    fseek(L.fp, 11, SEEK_SET);
    fprintf(L.fp, "%10u", L.pos);
    fflush(L.fp);
    fd = fileno(L.fp);
    fsync(fd);
    
  }

  /* Release lock */
  if (L.unlock) {
    L.unlock(L.udata);
  }

}
