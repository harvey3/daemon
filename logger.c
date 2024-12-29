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
#include <sys/prctl.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "log.h"

#define MAXLINE 2048
#define PATH_LOG "/var/log/daemon_logger"
#define PATH_HEARTBEAT "/var/log/daemon_heartbeat"

extern pid_t proc_daemon_pid;

struct logger_data ldata;
struct heartbeat_data hb_data;

    
void logger_init(char *path)
{
    FILE *fd;
    int fd_no;
    int sd;
	const struct sockaddr_un sun = {
		.sun_family = AF_LOCAL,
#ifdef HAVE_SA_LEN
		.sun_len = sizeof(sun),
#endif
		.sun_path = PATH_LOG,
	};

    memset(&ldata, 0, sizeof(struct logger_data));
    ldata.log_path = path;
    
    (void)unlink(sun.sun_path);
	
    
	sd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sd < 0)
		return;


	if (bind(sd, (struct sockaddr *)&sun, sizeof(sun)) < 0)
		return;

    ldata.sd = sd;
    
    fd = fopen(ldata.log_path, "rt+");
    if (!fd) {
        if (errno != ENOENT)
            return;

        fd = fopen(ldata.log_path, "w+");
        if (!fd) {
            return;
        }
        else {

            ldata.pos += fprintf(fd, "log offset:23        ");
            ldata.pos += fprintf(fd, "\n");
            fflush(fd);
            fd_no = fileno(fd);
            fsync(fd_no);

        }

    }
    else {
        fseek(fd, 11, SEEK_SET);
        fscanf(fd, "%u", &ldata.pos);
    }

    ldata.fd = fd;
    

}


void logger_loop(struct logger_data *data) {

    int len;
	char msg[MAXLINE + 1] = { 0 };

    prctl(PR_SET_NAME, "logger_daemon");

    while (1) {
        
        len = recvfrom(data->sd, msg, sizeof(msg) - 1, 0, NULL, NULL);
        if (len <= 0) {
            if (len < 0 && errno != EINTR && errno != EAGAIN)
                continue;
        }
        msg[len] = 0;

        if (data->fd) {


            if (data->pos > MAX_FILE_SIZE) {
                data->pos = 22;
                fseek(data->fd, 22, SEEK_SET);

            } else
                fseek(data->fd, data->pos, SEEK_SET);

            data->pos += fprintf(data->fd, "%s", msg);

            /* rewind(L.fp); */
            fseek(data->fd, 11, SEEK_SET);
            fprintf(data->fd, "%10u", data->pos);
            fflush(data->fd);
        }

    }
    

}

void heartbeat_init(void)
{
    int sd;
	const struct sockaddr_un sun = {
		.sun_family = AF_LOCAL,
#ifdef HAVE_SA_LEN
		.sun_len = sizeof(sun),
#endif
		.sun_path = PATH_HEARTBEAT,
	};

    memset(&hb_data, 0, sizeof(struct logger_data));
    
    (void)unlink(sun.sun_path);
    
	sd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sd < 0)
		return;


	if (bind(sd, (struct sockaddr *)&sun, sizeof(sun)) < 0)
		return;

    hb_data.sd = sd;
    

}

void heartbeat_loop(struct heartbeat_data *data) {

    int len;
    struct heartbeat_item item;
    fd_set fds;
    struct timeval timeout;
    int ret;
    int i;
    time_t now;
    int timeout_cnt = 0;
    
    
    prctl(PR_SET_NAME, "heartbeat_daemon");
    
    FD_ZERO(&fds);
    while (1) {

        FD_SET(data->sd, &fds);
        timeout.tv_sec = 1;
		timeout.tv_usec = 0;

        ret = select(data->sd + 1, &fds, NULL, NULL, &timeout);

        if (ret > 0) {

            if (FD_ISSET(data->sd, &fds)) {

                len = recvfrom(data->sd, &item, sizeof(item), 0, NULL, NULL);

                if (len > 0) {

                    for (i = 0; i < data->item_cnt; i++) {

                        if (data->items[i].pid == item.pid) {
                            data->items[i].cycle = item.cycle;
                            data->items[i].next_time = time(NULL);
                            break;
                        }

                    }

                    if (i == data->item_cnt) {
                        data->items[i].pid = item.pid;
                        data->items[i].cycle = item.cycle;
                        data->items[i].next_time = time(NULL);
                        data->item_cnt++;
                        
                    }
                    log_error("get %d heartbeat", item.pid);
                    

                }
                
            }
        } 

        now = time(NULL);
        for (i = 0; i < data->item_cnt; i++) {
            if ((now - data->items[i].next_time) > 2 * data->items[i].cycle) {
                log_error("pid %d heartbeat timeout", data->items[i].pid);
                kill(proc_daemon_pid, SIGRTMIN);
                data->item_cnt = 0;
                timeout_cnt++;
                if (timeout_cnt >= 3) 
                    system("reboot");
                sleep(10);
                break;
                
                
            }

        }
        

        
        
        

    }
    

}

