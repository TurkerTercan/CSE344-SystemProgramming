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

    sem_t *full = sem_open("semaphore_1", O_RDWR, 0666);
    sem_t *empty = sem_open("semaphore_2", O_RDWR, 0666);
    sem_t *mutex = sem_open("semaphore_3", O_RDWR, 0666);
    sem_t *first_vaccine = sem_open("semaphore_4", O_RDWR, 0666);
    sem_t *second_vaccine = sem_open("semaphore_5", O_RDWR, 0666);
    sem_t *ready_to_go = sem_open("semaphore_7", O_RDWR, 0666);
    int buffer_size;

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
     * Waiting clinic to increase below semaphore
     */
    sem_wait(ready_to_go);

    char vaccine;
    /**
     * Nurses should continue to read until a signal arrives or it reads EOF
     *
     * Nurses is very similar to producer in the producer-consumer problem
     * Only difference is it check whether the vaccine is type-1 or type-2 and increases it semaphore
     * for the vaccinator
     */
    while (read(*shared_fd, &vaccine, 1)) {
        if (terminate || kill_process)
            break;
        sem_wait(empty);
        if (terminate || kill_process)
            break;
        int vac_count_1 = 0;
        int vac_count_2 = 0;

        if (terminate || kill_process)
            break;
        sem_wait(mutex);
        if (terminate || kill_process)
            break;

        vac_count_1 = vaccine_types[0];
        vac_count_2 = vaccine_types[1];

        if (terminate || kill_process)
            break;
        for (int i = 0; i < buffer_size; ++i) {
            if (buffer_array[i] == 0) {
                int value = ((int)a) - ((int)vaccine);
                memcpy(buffer_array + i, &value, sizeof(int));
                break;
            }
        }
        if (terminate || kill_process)
            break;
        if (vaccine == '1') {
            sem_post(first_vaccine);
            vac_count_1++;
            memcpy(vaccine_types, &vac_count_1, sizeof(int));
        }
        else if (vaccine == '2') {
            sem_post(second_vaccine);
            vac_count_2++;
            memcpy(vaccine_types + 1, &vac_count_2, sizeof(int));
        }
        if (terminate || kill_process)
            break;
        sem_post(mutex);
        fprintf(stdout, "Nurse %s (pid=%d) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2\n", argv[1], getpid(), vaccine, vac_count_1, vac_count_2);
        fflush(stdout);
        sem_post(full);
    }

    munmap(shared_fd, sizeof(int));
    munmap(shared_buffer_size, sizeof(int));
    munmap(buffer_array, sizeof(int) * buffer_size);
    munmap(vaccine_types, sizeof(int) * 3);
    close(shm_file);
    close(shm_buffer_fd);
    close(shared_buffer_size_fd);
    close(vaccine_types_fd);
    sem_close(full);
    sem_close(empty);
    sem_close(mutex);
    sem_close(first_vaccine);
    sem_close(second_vaccine);
    sem_close(ready_to_go);
    return 0;
}