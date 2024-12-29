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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "log.h"

static struct log_data {
    const char *ident;
    int log_file;
    int log_opened;
    int log_connected;
    int level;
} sdata = LOG_DATA_INIT;

struct heartbeat_item 
{
    int pid;
    time_t next_time;
    time_t cycle;
    
};

struct heartbeat_data {
    int sd;
    int sd_connected;
    pthread_mutex_t heartbeat_lock;
    struct heartbeat_item items[MAX_THREAD_PER_PROCESS];
    int item_cnt;

};

static struct sockaddr_un hb_un = {
    .sun_family = AF_LOCAL,
    .sun_path = PATH_HEARTBEAT,
};

static struct heartbeat_data hb_data;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct sockaddr_un sun = {
    .sun_family = AF_LOCAL,
    .sun_path = PATH_LOG,
};

static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static void connectlog(struct log_data *data)
{
    struct sockaddr *sa;
    socklen_t len;

    sa  = (struct sockaddr *)&sun;


    len = sizeof(sun);


	if (data->log_file == -1 || fcntl(data->log_file, F_GETFL, 0) == -1) {
		data->log_file = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
		if (data->log_file == -1)
			return;

        data->log_opened = 1;
		data->log_connected = 0;
	}

    if (!data->log_connected) {

		if (connect(data->log_file, sa, len) == -1) {

			(void)close(data->log_file);
			data->log_file = -1;
            data->log_opened = 0;
            
		} else {
            

            data->log_connected = 1;
        }
        
    }



}


void log_open(const char *ident, int prio)
{
    
    pthread_mutex_lock(&log_mutex);

	if (ident != NULL)
		sdata.ident = ident;
    
    sdata.level = prio;
    
    connectlog(&sdata);

    pthread_mutex_unlock(&log_mutex);

}

static void
disconnectlog(struct log_data *data)
{
	/*
	 * If the user closed the FD and opened another in the same slot,
	 * that's their problem.  They should close it before calling on
	 * system services.
	 */
	if (data->log_file != -1) {
		(void)close(data->log_file);
		data->log_file = -1;
	}
	data->log_connected = 0;		/* retry connect */

}
void log_close(void)
{
    pthread_mutex_lock(&log_mutex);
    if (sdata.log_file > 0)
        (void)close(sdata.log_file);
	sdata.log_file = -1;
	sdata.log_connected = 0;
    sdata.log_opened = 0;
	sdata.ident = NULL;
    pthread_mutex_unlock(&log_mutex);

}

void log_log(int level, const char *file, int line, const char *fmt, ...)
{
    struct sockaddr *sa;
    
  if (level < sdata.level) {
    return;
  }
  sa  = (struct sockaddr *)&sun;
  /* Get current time */
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);

  /* Log to file */
  if (sdata.log_connected) {
    va_list args;
    char buf[TBUF_LEN];
    char tbuf[32];
    int n;
    int tries;
    int len;
    
    tbuf[strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';

    n = snprintf(buf, TBUF_LEN, "%s %-5s %s:%d: [%s]:", tbuf, level_names[level], file, line, sdata.ident);
    va_start(args, fmt);
    n += vsnprintf(buf + n, TBUF_LEN - n, fmt, args);
    va_end(args);


    len = sizeof(sun);
    
    for (tries = 0; tries < MAXTRIES; tries++) {
		if (sendto(sdata.log_file, buf, n, 0, sa, len) != -1)
			break;
		if (errno != ENOBUFS) {
			disconnectlog(&sdata);
			connectlog(&sdata);
		} else
			(void)usleep(1);
	}
    

  }



}

static void connect_heartbeat(struct heartbeat_data *data)
{
    struct sockaddr *sa;
    socklen_t len;

    sa  = (struct sockaddr *)&hb_un;


    len = sizeof(hb_un);


	if (data->sd == -1 || fcntl(data->sd, F_GETFL, 0) == -1) {
		data->sd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
		if (data->sd == -1)
			return;

		data->sd_connected = 0;
	}

    if (!data->sd_connected) {

		if (connect(data->sd, sa, len) == -1) {

			(void)close(data->sd);
			data->sd = -1;
            
		} else {

            data->sd_connected = 1;
        }
        
    }



}
static void disconnect_heartbeat(struct heartbeat_data *data)
{
	/*
	 * If the user closed the FD and opened another in the same slot,
	 * that's their problem.  They should close it before calling on
	 * system services.
	 */
	if (data->sd != -1) {
		(void)close(data->sd);
		data->sd = -1;
	}
	data->sd_connected = 0;		/* retry connect */

}

void heartbeat_init(void)
{
    memset(&hb_data, 0, sizeof(struct heartbeat_data));
    pthread_mutex_init(&hb_data.heartbeat_lock, NULL);
    hb_data.sd = -1;
    connect_heartbeat(&hb_data);

}

void heartbeat_timeout(int sec)
{

    int pid;
    int i;
    int found = 0;
    int tries;
    struct sockaddr *sa;
    int len;
    struct heartbeat_item item;
    time_t now;
    
    pid = getpid();
    
    pthread_mutex_lock(&hb_data.heartbeat_lock);
    
    for (i = 0; i < hb_data.item_cnt; i++) {

        if (hb_data.items[i].pid == pid) {
            found = 1;
            break;
        }
        

    }
    
    if (!found) {
        hb_data.items[hb_data.item_cnt].pid = pid;
        hb_data.items[hb_data.item_cnt].next_time = time(NULL) + sec;
        hb_data.item_cnt++;

    } else {
        now = time(NULL);
        if (hb_data.items[i].next_time <= now)
            hb_data.items[i].next_time = now + sec;
        else
            goto out;
        
    }
    

    sa = (struct sockaddr *)&hb_un;
    len = sizeof(hb_un);
    item.pid = pid;
    item.cycle = sec;
    for (tries = 0; tries < MAXTRIES; tries++) {
		if (sendto(hb_data.sd, &item, sizeof(item), 0, sa, len) != -1)
			break;
		if (errno != ENOBUFS) {
			disconnect_heartbeat(&hb_data);
			connect_heartbeat(&hb_data);
		} else
			(void)usleep(1);
	}

out:    
    pthread_mutex_unlock(&hb_data.heartbeat_lock);

}
