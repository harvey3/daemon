#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#include <errno.h>
#include "log.h"

#define M257_WDT_TIMEOUT_STATUS 0x2
#define M257_WDT_SOFTRESET_STATUS 0x1

#define A7_WDT_TIMEOUT_STATUS 0x20



#define ERR_EXIT(m) \
do\
{\
    perror(m);\
    exit(EXIT_FAILURE);\
}\
while (0);

static int timeout = 20;
static int bootstatus = 0;

void create_daemon(void);
void dump_last_kmsg();

int main(void)
{
    int fd;
    int ret;
    struct sched_param param;
    int min_prio;


    
    create_daemon();


    log_init("/usr/longertek/longer_daemon.log", LOG_ERROR);

    min_prio = sched_get_priority_min(SCHED_FIFO);
    if (min_prio < 0)
    {
        log_error("get min priority failed %d", min_prio);
        
    }
    param.sched_priority = min_prio;

    
    ret = sched_setscheduler(getpid(), SCHED_FIFO, &param);
    if (ret < 0)
    {
        log_error("set scheduler failed %d", ret);

    }

    fd = open("/dev/watchdog", O_RDWR);
    if (fd < 0) {
        log_error("open watchdog failed %d", errno);
        return 0;
    
    }

    ret = ioctl(fd, WDIOC_GETBOOTSTATUS, &bootstatus);
    if (ret) {
        log_error("get boot status failed %d", ret);
    }
    
#ifdef CONFIG_A7    
    if (bootstatus & A7_WDT_TIMEOUT_STATUS) {
#else
    if (bootstatus & M257_WDT_TIMEOUT_STATUS) {
#endif
        log_fatal("***wdt timeout! %d", bootstatus);
        log_fatal("****************dump****************");
        dump_last_kmsg();
        
    }
    
    ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
    if (ret)
    {
        log_error("wdt set timeout failed! %d", ret);
        return 0;
    }
    
    while(1){
        ret = ioctl(fd, WDIOC_KEEPALIVE, 0);
        if (ret)
        {
            log_error("feed wdt failed %d", ret);
            
        }
        sleep(2);
        
    }
    return 0;
}
void create_daemon(void)
{
    pid_t pid;
    int fd1, fd2, fd3;
    /* FILE *file; */
    
    pid = fork();
    if( pid == -1)
        ERR_EXIT("fork error");
    if(pid > 0 )
        exit(EXIT_SUCCESS);
    if(setsid() == -1)
        ERR_EXIT("SETSID ERROR");
    chdir("/");
        
    close(0);
    close(1);
    close(2);

    
    
    fd1 = open("/dev/null", O_RDWR);

        
    fd2 = dup(0);

    fd3 = dup(0);
    
    /* test fd dup */
#if 0    
    file = fopen("/tmp/dup.log", "a+");
    fprintf(file, "fd = %d\n", fd1);
    fprintf(file, "fd = %d\n", fd2);
    fprintf(file, "fd = %d\n", fd3);
    fclose(file);
#endif
    umask(0);
    signal(SIGCHLD,SIG_IGN);
    
    return;
}

void dump_last_kmsg()
{

    FILE* kmsg_fd;
    int len;
    int size;
    char *buf;
    
    errno = 0;
    kmsg_fd = fopen("/proc/last_kmsg", "r");
    if (!kmsg_fd) {
        
        log_error("open last_kmsg failed %d", errno);
        return;

    }
    
    fseek(kmsg_fd, 0, SEEK_END);
    
    size = ftell(kmsg_fd);

    if (size < 0)
    {
        log_error("failed to get kmsg size %d", size);
        fclose(kmsg_fd);
        return;

    }
    fseek(kmsg_fd, 0, SEEK_SET);

    buf = malloc(size);
    if (!buf)
    {
        log_error("failed to malloc buffer %d");
        fclose(kmsg_fd);
        return;

    }
    
    len = fread(buf, size, 1, kmsg_fd);
    if (len >= 0) {

        copy_log(buf, size);
        
    } else {

        log_error("failed to read kmsg %d", len);
    }
    free(buf);
    fclose(kmsg_fd);

}
