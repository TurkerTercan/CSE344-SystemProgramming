#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define MAXFILELENGTH 256
#define true 1
#define false 0
#define SEM_INIT_NAME "semaphoring"

struct potato{
    pid_t process;
    int total_switch;
    int i;
};

int terminated = false;

void terminate_program(char *memory, char *names, char *semaphore, int s_flag, int f_flag, int m_flag, int print_flag);

void get_command_line_arguments(int argc, char *argv[], int b_flag, int *potato_switches,
                                char **nameOfTheSharedMemory, int *s_flag, char **fileWithFifoNames, int *f_flag,
                                char **namedSemaphore, int *m_flag);

void free_fifo_names(int MAX_N, char **fifos);

void interrupt_handler(int signum) {
    if (signum == SIGINT)
        terminated = true;
}


int main(int argc, char* argv[]) {
    int potato_switches;
    int ith_process;
    int b_flag = 0;

    char* nameOfTheSharedMemory;
    int s_flag = 0;
    char* fileWithFifoNames;
    int f_flag = 0;
    char* namedSemaphore;
    int m_flag = 0;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1){
        perror("SIGACTION SIGINT");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    get_command_line_arguments(argc, argv, b_flag, &potato_switches, &nameOfTheSharedMemory, &s_flag,
                               &fileWithFifoNames, &f_flag,
                               &namedSemaphore, &m_flag);

    int fd_fifonames = open(fileWithFifoNames, O_RDONLY, 0666);


    char temp = '*';
    char *temp_loc = (char*) malloc(MAXFILELENGTH * sizeof(char));
    int x = 0;
    int y = 0;
    int MAX_N = 1;
    while (temp != '\0')
    {
        int size = read(fd_fifonames, &temp, 1);
        if(size <= 0)
            break;
        if (temp == '\n')
            MAX_N++;
    }

    char **fifos = (char **) malloc(MAX_N * sizeof(char *));
    temp = '*';

    lseek(fd_fifonames, 0, SEEK_SET);

    while (temp != '\0')
    {
        int size = read(fd_fifonames, &temp, 1);
        if(size <= 0)
            break;
        if (temp != '\n' && temp != '\0') {
            temp_loc[x++] = temp;
        } else if (temp == '\n' || temp == '\0') {
            temp_loc[x++] = '\0';
            char *temp_mal = (char *) malloc(x * sizeof(char));
            strcpy(temp_mal, temp_loc);
            fifos[y++] = temp_mal;
            x = 0;
            free(temp_loc);
            temp_loc = (char*) malloc(MAXFILELENGTH * sizeof(char));
        }
    }
    temp_loc[x++] = '\0';
    char *temp_mal = (char *) malloc(x * sizeof(char));
    strcpy(temp_mal, temp_loc);
    fifos[y++] = temp_mal;
    free(temp_loc);

    close(fd_fifonames);

    struct potato *potatoes;    // I will use first potato to indicate which fifoname should choose from the file
    int shared_mem_fd;
    shared_mem_fd = shm_open(nameOfTheSharedMemory, O_RDWR | O_CREAT | O_EXCL, 0666);

    sem_t *init_sem = sem_open(SEM_INIT_NAME, O_CREAT, 0666);
    sem_t *shared_sem = sem_open(namedSemaphore, O_CREAT, 0666, 1);

    /**
     * This if condition specifies that the process is the first process executing this program
     * so i implemented such a way that it needs create and resize for shared memory segment
     * initialize potatoes array
     * and make all fifo files for further use
     * The other processes needs the wait until first process finish this part so i created semaphore for this
     */
    if (shared_mem_fd != -1){
        if (ftruncate(shared_mem_fd, sizeof(struct potato) * (2 * MAX_N + 1))) {
            perror("FTRUNCATE");
            close(shared_mem_fd);
            sem_unlink(SEM_INIT_NAME);
            free_fifo_names(MAX_N, fifos);
            terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
        }
        potatoes = mmap(NULL, (sizeof(struct potato) * (2 * MAX_N + 1)), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
        if (potatoes == MAP_FAILED) {
            perror("MMAP");
            close(shared_mem_fd);
            sem_unlink(SEM_INIT_NAME);
            free_fifo_names(MAX_N, fifos);
            terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
        }
        struct potato n;
        ith_process = 0;
        n.total_switch = 0;
        n.process = -1;
        n.i = -1;
        memcpy(potatoes, &n, sizeof(struct potato));

        struct potato process;
        process.process = getpid();
        memcpy(potatoes + 1 + MAX_N, &process, sizeof(struct potato));

        for (int i = 1; i < MAX_N + 1; ++i) {
            struct potato temp;
            temp.total_switch = -1;
            temp.process = 0;
            temp.i = -1;
            memcpy(potatoes + i, &temp, sizeof(struct potato));
        }

        for (int i = 0; i < MAX_N; ++i) {
            if ((mkfifo(fifos[i], 0666) == -1) && errno != EEXIST) {
                perror("MAKEFIFO");
                sem_unlink(SEM_INIT_NAME);
                close(shared_mem_fd);
                terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
            }
        }
        for (int i = 0; i < MAX_N - 1; i++) {
            sem_post(init_sem);
        }
    }
    /**
     * If it is not the first process then, it only needs to attach itself to shared memory
     * I used first index of the array as indicate that how many processes are there
     * so once a process created, it should increment that first index of the shared memory
     */
    else {
        sem_wait(init_sem);
        sem_wait(shared_sem);

        shared_mem_fd = shm_open(nameOfTheSharedMemory, O_RDWR, 0666);
        if (shared_mem_fd == -1) {
            perror("SHM_OPEN");
            sem_unlink(SEM_INIT_NAME);
            close(shared_mem_fd);
            free_fifo_names(MAX_N, fifos);
            terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
        }
        potatoes = mmap(NULL, (sizeof(struct potato) * (2 * MAX_N + 1)), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
        ith_process = potatoes[0].total_switch + 1;
        if (ith_process >= MAX_N)
        {
            fprintf(stdout, "Unable to create new potato process!\n%d is returning.", getpid());
            close(shared_mem_fd);
            terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
        }

        struct potato temp;
        temp.total_switch = ith_process;
        temp.process = potatoes[0].process;
        temp.i = 0;

        memcpy(potatoes, &temp, sizeof(struct potato));

        struct potato process;
        process.process = getpid();
        memcpy(potatoes + 1 + MAX_N + ith_process, &process, sizeof(struct potato));
        sem_post(shared_sem);
    }

    /**
     * Once a process created, it should be able to write any other process' fifo
     * and any other process should be able to write its own fifo.
     * We have to communicate between every process with these fifos
     * I already created fifos with the first processes, so i just need to
     * open other fifos as write only and its process as read only.
     */
    int fifo_fd_read;
    int fifo_fd_write[MAX_N];
    for (int i = 0; i < MAX_N && !terminated; ++i) {
        if (i != ith_process) {
            fifo_fd_write[i] = open(fifos[i], O_WRONLY);
            if (fifo_fd_write[i] == -1)
                perror("FIFO WRITE OPEN");
        } else {
            fifo_fd_read = open(fifos[ith_process], O_RDONLY);
            if (fifo_fd_read == -1) {
                perror("FIFO READ OPEN");
                close(shared_mem_fd);
                free_fifo_names(MAX_N, fifos);
                terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
            }
        }
    }
    if (terminated) {
        fprintf(stderr,"Program terminated by SIGINT signal\n");
        for (int i = 0; i <= potatoes[0].total_switch; ++i) {
            if (i != ith_process)
                kill(potatoes[i + 1 + MAX_N].process, SIGINT);
        }

        fflush(stderr);
        free_fifo_names(MAX_N, fifos);
        close(fifo_fd_read);
        close(shared_mem_fd);
        munmap(potatoes, sizeof(struct potato) * (MAX_N + 1) * 5);
        shm_unlink(nameOfTheSharedMemory);
        sem_close(init_sem);
        sem_close(shared_sem);
        sem_unlink(SEM_INIT_NAME);
        sem_unlink(namedSemaphore);

        terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
    }

    /**
     * If created process has a potato, first it should write this potato to shared memory
     * then, it randomly selects a process and writes potato's originally created process id
     */
    if (potato_switches > 0) {
        struct potato my_potato;
        my_potato.total_switch = potato_switches;
        my_potato.process = getpid();
        my_potato.i = 1;

        sem_wait(shared_sem);
        for (int i = 1; i < MAX_N + 1; ++i) {
            if (potatoes[i].total_switch == -1) {
                memcpy(potatoes + i, &my_potato, sizeof(struct potato));
                break;
            }
        }
        sem_post(shared_sem);

        int i = -1;
        while (i == -1 || i == ith_process)
            i = rand() % MAX_N;

        write(fifo_fd_write[i], &my_potato.process, sizeof(pid_t));
        fprintf(stdout, "pid=%d sending potato number %d to %s; this is switch number %d\n", getpid(), my_potato.process, fifos[i], my_potato.i);
        fflush(stdout);
    }


    /**
     * Process reads it own fifo, it stay on the blocked process until a message arrives
     * message contains a pid number, it finds the potato in the shared memory
     * if the potato has not been cooled down yet, send the potato to random process
     * if all the potatoes have been cooled down, send a pid number that is 0 for
     * all processes to indicate that they should terminate
     */
    while (!terminated) {
        pid_t incoming_message;
        read(fifo_fd_read, &incoming_message, sizeof(pid_t));

        if (incoming_message == 0)
            break;

        int shared_segment_id;
        for (shared_segment_id = 1; shared_segment_id < MAX_N + 1; ++shared_segment_id) {
            if (potatoes[shared_segment_id].process == incoming_message)
                break;
        }

        if (potatoes[shared_segment_id].i == potatoes[shared_segment_id].total_switch) {
            fprintf(stdout, "pid=%d; potato number %d has cooled down\n", getpid(), potatoes[shared_segment_id].process);
            fflush(stdout);

            /**
             * What if all the potatoes are cooled down
             * Check all shared memory segments to find out if there is any potato that is has not cooled down yet.
             * If there isn't, terminate all other processes
             */
            int isFinished = true;
            for (int i = 1; i < MAX_N + 1; ++i) {
                if (potatoes[i].i != potatoes[i].total_switch) {
                    isFinished = false;
                    break;
                }
            }

            /**
             * I write every fifo a pid number 0, which means all the processes should terminate immediately.
             */
            if (isFinished == true) {
                for (int i = 0; i < MAX_N; ++i) {
                    if (i != ith_process) {
                        pid_t destroy = 0;
                        write(fifo_fd_write[i], &destroy, sizeof(pid_t));
                    }
                }
                break;
            }
        }
        else {
            fprintf(stdout, "pid=%d receiving potato number %d from %s\n", getpid(), incoming_message, fifos[ith_process]);
            fflush(stdout);

            struct potato hot_potato = potatoes[shared_segment_id];
            hot_potato.i = hot_potato.i + 1;

            sem_wait(shared_sem);
            memcpy(potatoes + shared_segment_id,&hot_potato, sizeof(struct potato));
            sem_post(shared_sem);


            int i = -1;
            while (i == -1 || i == ith_process)
                i = rand() % MAX_N;

            write(fifo_fd_write[i], &hot_potato.process, sizeof(pid_t));
            fprintf(stdout, "pid=%d sending potato number %d to %s; this is switch number %d\n", getpid(), hot_potato.process, fifos[i], hot_potato.i);
            fflush(stdout);
        }
    }

    if (terminated) {
        fprintf(stderr,"Program terminated by SIGINT signal\n");
        for (int i = 0; i <= potatoes[0].total_switch; ++i) {
            if (i != ith_process)
                kill(potatoes[i + 1 + MAX_N].process, SIGINT);
        }

        fflush(stderr);
        free_fifo_names(MAX_N, fifos);
        close(fifo_fd_read);
        close(shared_mem_fd);
        munmap(potatoes, sizeof(struct potato) * (MAX_N + 1) * 5);
        shm_unlink(nameOfTheSharedMemory);
        sem_close(init_sem);
        sem_close(shared_sem);
        sem_unlink(SEM_INIT_NAME);
        sem_unlink(namedSemaphore);

        terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
    }

    free_fifo_names(MAX_N, fifos);
    close(fifo_fd_read);
    close(shared_mem_fd);
    munmap(potatoes, sizeof(struct potato) * (2 * MAX_N + 1) * 5);
    shm_unlink(nameOfTheSharedMemory);
    sem_close(init_sem);
    sem_close(shared_sem);
    sem_unlink(SEM_INIT_NAME);
    sem_unlink(namedSemaphore);
    terminate_program(nameOfTheSharedMemory, fileWithFifoNames, namedSemaphore, s_flag, f_flag, m_flag, 0);
    return 0;
}

void free_fifo_names(int MAX_N, char **fifos) {
    for (int i = 0; i < MAX_N; ++i) {
        free(fifos[i]);
    }
    free(fifos);
}

/**
 * Getting command line arguments with getopt function
 */
void get_command_line_arguments(int argc, char *argv[], int b_flag, int *potato_switches,
                                char **nameOfTheSharedMemory, int *s_flag, char **fileWithFifoNames, int *f_flag,
                                char **namedSemaphore, int *m_flag) {
    int opt;
    while((opt = getopt(argc, argv, "b:s:f:m:")) != - 1) {
        switch (opt) {
            case 'b':
                if (optarg != NULL)
                    (*potato_switches) = atoi(optarg);
                if ((*potato_switches) < 0)
                    terminate_program((*nameOfTheSharedMemory), (*fileWithFifoNames), (*namedSemaphore), (*s_flag), (*f_flag), (*m_flag), 1);
                b_flag = 1;
                break;
            case 's':
                (*nameOfTheSharedMemory) = (char*) malloc((strlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy((*nameOfTheSharedMemory), optarg);
                (*s_flag) = 1;
                break;
            case 'f':
                (*fileWithFifoNames) = (char*) malloc((strlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy((*fileWithFifoNames), optarg);
                (*f_flag) = 1;
                break;
            case 'm':
                (*namedSemaphore) = (char*) malloc((strlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy((*namedSemaphore), optarg);
                (*m_flag) = 1;
                break;
            default:
                terminate_program((*nameOfTheSharedMemory), (*fileWithFifoNames), (*namedSemaphore), (*s_flag), (*f_flag), (*m_flag), 1);
        }
    }
    if (!b_flag || !(*s_flag) || !(*f_flag) || !(*m_flag))
        terminate_program((*nameOfTheSharedMemory), (*fileWithFifoNames), (*namedSemaphore), (*s_flag), (*f_flag), (*m_flag), 1);
}

void terminate_program(char *memory, char *names, char *semaphore, int s_flag, int f_flag, int m_flag, int print_flag) {
    if (print_flag == 1) {
        fprintf(stderr, "Error: An unexpected argument occurred\n");
        fprintf(stderr,"Usage: ./player –b haspotatoornot –s nameofsharedmemory –f filewithfifonames –m namedsemaphore\n");
    }
    if (s_flag)
        free(memory);
    if (f_flag)
        free(names);
    if (m_flag)
        free(semaphore);
    exit(EXIT_FAILURE);
}
