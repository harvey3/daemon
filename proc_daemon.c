#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/prctl.h>
#include <signal.h>

#include "log.h"


static int all_process_booted = 0;

#define ALL_PROCESS_CNT 5
#define MAX_CHECK_DELAY 20

#define MAX_AIRCON_PIDS 8
#define MAX_QT_PIDS 4
#define MAX_SCAN_ERR 3
    
struct process_snapshot 
{
    int qt_pid_cnt;
    int aircon_pid_cnt;

    int qt_pids[MAX_QT_PIDS];
    int aircon_pids[MAX_AIRCON_PIDS];

};

    
int pid_to_process_index(int *pids, int pid)
{

    int i;

    for (i = 0; i < sizeof(pids); i++)
    {
        
        if (pid == pids[i])
            break;
        else if (pid < pids[i])
        {
            memmove(pids + i + 1, pids + i, sizeof(pids) - i - 1);
            pids[i] = pid;
            break;
            
            
        }
        else if (pids[i] == 0)
        {
            
            pids[i] = pid;
            break;
            
        }
        
    }
    
    
    return i == 10 ? -1 : i;
    
}

/* find longertek process name */
/* return <0: not found        */
/*         0: QT_LAC           */
/*         1: TrainAircon process     */

int find_longertek_cmdline(char *cmdline)
{
    /* if (strncmp(cmdline, "/usr/longertek/", 15)) */
    /*     return -1; */

    if (strstr(cmdline, "QT_LAC"))
        return 0;

    else if (strstr(cmdline, "TrainAircon"))
        return 1;
    else
        return -1;

}

int start_process()
{

    pid_t pid;
    
    if ((pid = fork()) < 0) {
        log_error("fork error %d", errno);
        return -1;
        
    } else if (pid == 0) {
#ifdef CONFIG_A7    
        execl("/etc/init.d/S85qt.sh", "/etc/init.d/S85qt.sh", 
             NULL, NULL);

#else
        execl("/usr/share/zhiyuan/zylauncher/start_zylauncher", "/usr/share/zhiyuan/zylauncher/start_zylauncher", NULL, NULL);
#endif   
        exit(0);
        
    } 
        
    
    return 0;


}
/* char test_buf[2000]; */

int restart_process(struct process_snapshot *ps,
    struct process_snapshot *ps_orig)
{
    char pid[100];
    char *tmp;
    int i;
    
    tmp = pid;
    memset(pid, 0, sizeof(pid));
    for (i = 0; i < ps_orig->aircon_pid_cnt; i++)
    {

        tmp += sprintf(tmp, " %d", ps_orig->aircon_pids[i]);
                
    }
            
    log_error("original aircon pids:%s", pid);                

    tmp = pid;
    memset(pid, 0, sizeof(pid));
    for (i = 0; i < ps->aircon_pid_cnt; i++) {
        kill(ps->aircon_pids[i], SIGKILL);
        tmp += sprintf(tmp, " %d", ps->aircon_pids[i]);
                
    }
            
    log_error("existing aircon pids:%s", pid);
            
    tmp = pid;
    memset(pid, 0, sizeof(pid));
    for (i = 0; i < ps_orig->qt_pid_cnt; i++)
    {

        tmp += sprintf(tmp, " %d", ps_orig->qt_pids[i]);
    }
    log_error("original qt pids:%s", pid);
            
    tmp = pid;
    memset(pid, 0, sizeof(pid));
    for (i = 0; i < ps->qt_pid_cnt; i++) {
        kill(ps->qt_pids[i], SIGKILL);
        tmp += sprintf(tmp, " %d", ps->qt_pids[i]);

    }
    log_error("existing qt pids:%s", pid);                
            
    all_process_booted = 0;
    memset(ps_orig, 0, sizeof(*ps_orig));
    sleep(3);
    start_process();
    sleep(MAX_CHECK_DELAY);
    return 0;
    
}

int scan_process(struct process_snapshot *ps)
{
    struct dirent *entry;

    int fd;
    int len;
    char filename[sizeof("/proc/%u/task/%u/cmdline") + sizeof(int)*3 * 2];
    DIR *dirp;
    char buf[100];
    int ret;
    int pid;
    
    dirp = opendir("/proc");
    if (!dirp) {
        log_fatal("open /proc failed %d", errno);
        
        return -1;
    }
    
	while ((entry = readdir(dirp)) != NULL) {
        

	    if (isdigit(entry->d_name[0])) {
            
            sprintf(filename, "/proc/%s/cmdline", entry->d_name);
            pid = atoi(entry->d_name);
            
            fd = open(filename, O_RDONLY);
            if (fd < 0) {

                continue;
            }
            
            memset(buf, 0, sizeof(buf));
            len = read(fd, buf, 100);

            if (len < 0) {

                close(fd);
                continue;
            }
            else if (len == 0)
            {

                close(fd);
                continue;
            }
            
            
            close(fd);
            
            ret = find_longertek_cmdline(buf);

            if (ret < 0)
                continue;
            else if (ret == 0)
            {
                
                ps->qt_pids[ps->qt_pid_cnt] = pid;
                ps->qt_pid_cnt++;

                /* if (ps->qt_pid_cnt > 1) { */
                    
                /*     log_error("cmdline is %s", buf); */
                /*     sprintf(filename, "/proc/%s/status", entry->d_name); */
                /*     fd = open(filename, O_RDONLY); */
                /*     len = read(fd, test_buf, 2000); */
                /*     copy_log(test_buf, len); */
                /*     close(fd); */
                    
                /* } */
                
            }
            
            else if (ret == 1)
            {

                ps->aircon_pids[ps->aircon_pid_cnt] = pid;
                ps->aircon_pid_cnt++;

                
                
            }

        }
        
    }

    closedir(dirp);
    return ps->qt_pid_cnt + ps->aircon_pid_cnt;
    

}

int proc_daemon()
{
    struct process_snapshot ps;
    struct process_snapshot ps_orig;
    int i, j;
    int qt_pid_matched = 0;
    int aircon_pid_matched = 0;
    int cnt;
    int err_cnt = 0;

    prctl(PR_SET_NAME, "proc_daemon");
    sleep(MAX_CHECK_DELAY);
    
    memset(&ps_orig, 0, sizeof(ps_orig));
    
    while (1) {
        
        sleep(2);
        memset(&ps, 0, sizeof(ps));
        
        cnt = scan_process(&ps);
        if (cnt < 0)
            continue;
    
    
        if (cnt == ALL_PROCESS_CNT && !all_process_booted)
        {
            all_process_booted = 1;
            memcpy(&ps_orig, &ps, sizeof(ps));
        
        
        }
    
        else if (cnt < ALL_PROCESS_CNT && all_process_booted)
        {
            err_cnt++;

            if (err_cnt < MAX_SCAN_ERR)
                continue;
            
            log_error("process crashing detected, pid cnt %d", cnt);
            err_cnt = 0;
            restart_process(&ps, &ps_orig);
            

        }
        else if (cnt < ALL_PROCESS_CNT && !all_process_booted)
        {

            err_cnt++;

            if (err_cnt < MAX_SCAN_ERR)
                continue;

            err_cnt = 0;

            log_error("process crashing detected at first detect, pid cnt %d", cnt);
            restart_process(&ps, &ps_orig);
            
        } else {

            if (all_process_booted)
            {
                for (i = 0; i < ps_orig.qt_pid_cnt; i++)
                {
                    qt_pid_matched = 0;
                    
                    for (j = 0; j <  ps.qt_pid_cnt; j++)
                    {
                        if (ps_orig.qt_pids[i] == ps.qt_pids[j])
                        {
                            
                            qt_pid_matched = 1;
                            break;
                            
                        }
                        


                    }

                    if (qt_pid_matched == 0) {
                                    
                        err_cnt++;
                        if (err_cnt == MAX_SCAN_ERR) {
                            err_cnt = 0;
                            goto restart;

                        }
                        
                    }
                    

                }
                

                for (i = 0; i < ps_orig.aircon_pid_cnt; i++)
                {
                    aircon_pid_matched = 0;
                    
                    for (j = 0; j <  ps.aircon_pid_cnt; j++)
                    {
                        if (ps_orig.aircon_pids[i] == ps.aircon_pids[j])
                        {
                            
                            aircon_pid_matched = 1;
                            break;
                            
                        }
                        


                    }

                    if (aircon_pid_matched == 0) {
                        err_cnt++;
                        if (err_cnt == MAX_SCAN_ERR) {
                            err_cnt = 0;
                            goto restart;
                        }
                        
                    }
                    

                }
                
                err_cnt = 0;
                continue;

            restart:
                log_error("process pids not match the original %d %d", qt_pid_matched, aircon_pid_matched);
                restart_process(&ps, &ps_orig);
                 

            }
            
            

        }
        
        
    
    }
    
}
