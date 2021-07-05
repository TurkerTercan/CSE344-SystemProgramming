//
// Created by ttwicer on 3.06.2021.
//

#ifndef FINAL_SERVER_H
#define FINAL_SERVER_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#define true 1
#define false 0

struct pool_t {
    int id;
    pthread_t pool_thread;
    int busy_flag;
    int connection_socket_fd;
    pthread_cond_t cond;
};

#define SIZE 500000
#define MAX 2048
#define SA struct sockaddr

/* Bit-mask values for 'flags' argument of becomeDaemon() */
#define BD_NO_CHDIR 01
/* Don't chdir("/") */
#define BD_NO_CLOSE_FILES 02
/* Don't close all open files */
#define BD_NO_REOPEN_STD_FDS 04
/* Don't reopen stdin, stdout, and stderr to
*
/dev/null */
#define BD_NO_UMASK0 010 /* Don't do a umask(0) */
#define BD_MAX_CLOSE 8192 /* Maximum file descriptors to close if
* sysconf(_SC_OPEN_MAX) is indeterminate */

int becomeDaemon(int flags);

#endif //FINAL_SERVER_H
