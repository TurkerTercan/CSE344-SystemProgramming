#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "data_types.h"

/**
 * boolean for the SIGINT signal
 */
int terminate = 0;
/**
 * boolean for the SIGUSR1 signal
 */
int kill_process = 0;

void interrupt_handler(int signum) {
    if (signum == SIGINT)
        kill_process = 1;
    else if (signum == SIGUSR1)
        terminate = 1;
}


int main(int argc, char* argv[]) {
    int buffer_size;
    int citizen_array_size = atoi(argv[2]);
    int fifos[citizen_array_size];

    sem_t *full = sem_open("semaphore_1", O_RDWR, 0666);
    sem_t *empty = sem_open("semaphore_2", O_RDWR, 0666);
    sem_t *mutex = sem_open("semaphore_3", O_RDWR, 0666);
    sem_t *first_vaccine = sem_open("semaphore_4", O_RDWR, 0666);
    sem_t *second_vaccine = sem_open("semaphore_5", O_RDWR, 0666);
    sem_t *second_mutex = sem_open("semaphore_6", O_RDWR, 0666);
    sem_t *ready_to_go = sem_open("semaphore_7", O_RDWR, 0666);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("SIGACTION");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("SIGACTION");
        exit(EXIT_FAILURE);
    }

    int shm_file = shm_open("file_descriptor", O_RDWR, 0666);
    int *shared_fd = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_file, 0);

    int shared_buffer_size_fd = shm_open("shared_buffer_size", O_RDWR, 0666);
    int *shared_buffer_size = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shared_buffer_size_fd, 0);
    buffer_size = *shared_buffer_size;

    int shm_buffer_fd = shm_open("shared_buffer", O_RDWR, 0666);
    int *buffer_array = mmap(NULL, (sizeof(int) * buffer_size), PROT_READ | PROT_WRITE, MAP_SHARED, shm_buffer_fd, 0);

    int citizen_array_fd = shm_open("citizen_array", O_CREAT | O_RDWR, 0666);
    struct reg_citizen *citizen_array = mmap(NULL, (sizeof(struct reg_citizen) * citizen_array_size), PROT_READ | PROT_WRITE, MAP_SHARED, citizen_array_fd, 0);

    int vaccine_types_fd = shm_open("vaccine_types", O_RDWR, 0666);
    int *vaccine_types = mmap(NULL, (sizeof(int) * 3), PROT_READ | PROT_WRITE, MAP_SHARED, vaccine_types_fd, 0);

    /**
     * When the all of the initialization part has completed, now we can send a message to the clinic that is
     * we're ready for call of duty.
     */

    int zero = 0;
    int clinic = open(clinic_fifo, O_WRONLY);
    write(clinic, &zero, sizeof(int));

    /**
     * In order to handle large citizen and vaccinators, we should wait every citizen and vaccinators to ready for
     * fifo read and write. If we don't do that, may a race condition happens and program stucks in a deadlock because
     * before a vaccinator creation, there is a chance that a citizen had all needed vaccine shot and terminated so
     * vaccinator wants to write it's fifo but other side is already closed and vaccinator waits forever.
     */
    for (int i = 0; i < citizen_array_size; ++i) {
        char fifoname[INTEGERBITSIZE];
        sprintf(fifoname, "/tmp/%d", citizen_array[i].pid);
        if (citizen_array[i].count != 0)
            fifos[i] = open(fifoname, O_WRONLY);
    }

    /**
     * Waiting clinic to increase below semaphore
     */
    sem_wait(ready_to_go);

    int count = 0;
    int vac_count_1 = 0;
    int vac_count_2 = 0;

    /**
     * It's a little bit different than the classic approach. I divided the vaccinator code to different two mutexes, because
     * first mutex protects critical region that is shared between nurses
     * second mutex protects shared memory segments that has been shared between vaccinators
     */
    while (!terminate && !kill_process) {
        if (terminate || kill_process)
            break;
        sem_wait(full);
        if (terminate || kill_process)
            break;
        sem_wait(first_vaccine);
        if (terminate || kill_process)
            break;
        sem_wait(second_vaccine);
        if (terminate || kill_process)
            break;
        sem_wait(mutex);
        if (terminate || kill_process)
            break;
        int first, second;
        for(first = 0; first < buffer_size; first++) {
            if(buffer_array[first] == 1)
                break;
        }
        for(second = 0; second < buffer_size; second++) {
            if(buffer_array[second] == 2)
                break;
        }
        memset(buffer_array + first, 0, sizeof(int));
        memset(buffer_array + second, 0, sizeof(int));
        vac_count_1 = vaccine_types[0];
        vac_count_2 = vaccine_types[1];

        vac_count_1--;
        vac_count_2--;
        memcpy(vaccine_types, &vac_count_1, sizeof(int));
        memcpy(vaccine_types + 1, &vac_count_2, sizeof(int));
        sem_post(mutex);

        if (terminate || kill_process)
            break;
        sem_wait(second_mutex);
        if (terminate || kill_process)
            break;
        struct reg_citizen chosen;
        int i;
        int remaining = 0;

        for (i = 0; i < citizen_array_size; ++i) {
            if (citizen_array[i].count != 0 && citizen_array[i].busy_flag == 0) {
                chosen = citizen_array[i];
                break;
            }
        }
        for (int j = 0; j < citizen_array_size; j++) {
            if (citizen_array[i].count != 0)
                remaining++;
        }

        chosen.busy_flag = 1;
        memcpy(citizen_array + i, &chosen, sizeof(struct reg_citizen));

        int message[3];
        message[0] = vac_count_1;
        message[1] = vac_count_2;
        message[2] = remaining;

        if (terminate || kill_process)
            break;
        write(fifos[i], &message, sizeof(int) * 3);
        fprintf(stdout, "Vaccinator %s (pid=%d) is inviting citizen pid=%d to the clinic\n", argv[1], getpid(), chosen.pid);
        fflush(stdout);
        chosen.busy_flag = 0;
        chosen.count = chosen.count - 1;
        if (chosen.count == 0) {
            close(fifos[i]);
        }
        memcpy(citizen_array + i, &chosen, sizeof(struct reg_citizen));

        sem_post(second_mutex);
        sem_post(empty);
        count++;
    }

    for (int i = 0; i < citizen_array_size; ++i) {
        close(fifos[i]);
    }
    munmap(shared_fd, sizeof(int));
    munmap(shared_buffer_size, sizeof(int));
    munmap(buffer_array, sizeof(int) * buffer_size);
    munmap(vaccine_types, sizeof(int) * 3);
    munmap(citizen_array, sizeof(struct reg_citizen) * citizen_array_size);
    close(shm_file);
    close(shm_buffer_fd);
    close(shared_buffer_size_fd);
    close(citizen_array_fd);
    close(vaccine_types_fd);
    sem_close(full);
    sem_close(empty);
    sem_close(mutex);
    sem_close(first_vaccine);
    sem_close(second_vaccine);
    sem_close(second_mutex);
    sem_close(ready_to_go);
    close(clinic);
    return count;
}