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
 * boolean for SIGINT signal
 */
int kill_process = 0;

/**
 * Signal handler
 * @param signum SIGINT
 */
void interrupt_handler(int signum) {
    if (signum == SIGINT)
        kill_process = 1;
}

int main(int argc, char* argv[]) {
    char fifoname[INTEGERBITSIZE];
    sprintf(fifoname, "/tmp/%d", getpid());

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("SIGACTION");
        exit(EXIT_FAILURE);
    }

    int count = 0;
    int citizen_array_size = atoi(argv[3]);
    int shot_count = atoi(argv[1]);


    sem_t *first_vaccine = sem_open("semaphore_4", O_RDWR, 0666);
    sem_t *second_vaccine = sem_open("semaphore_5", O_RDWR, 0666);
    sem_t *ready_to_go = sem_open("semaphore_7", O_RDWR, 0666);

    int citizen_array_fd = shm_open("citizen_array", O_CREAT | O_RDWR, 0666);
    struct reg_citizen *citizen_array = mmap(NULL, (sizeof(struct reg_citizen) * citizen_array_size), PROT_READ | PROT_WRITE, MAP_SHARED, citizen_array_fd, 0);

    int vac_count_1;
    int vac_count_2;
    int remaining;

    /**
     * When the all of the initialization part has completed, now we can send a message to the clinic that is
     * citizen is ready to receive vaccine
     */

    int clinic = open(clinic_fifo, O_WRONLY);
    write(clinic, &count, sizeof(int));

    /**
     * In order to handle large citizen and vaccinators, we should wait every citizen and vaccinators to ready for
     * fifo read and write. If we don't do that, may a race condition happens and program stucks in a deadlock because
     * before a vaccinator creation, there is a chance that a citizen had all needed vaccine shot and terminated so
     * vaccinator wants to write it's fifo but other side is already closed and vaccinator waits forever.
     */

    int my_fifo = open(fifoname, O_RDONLY);

    /**
     * Waiting clinic to increase below semaphore
     */
    sem_wait(ready_to_go);


    while (count != shot_count) {
        int message[3];
        read(my_fifo, &message, sizeof(int) * 3);
        count++;
        char *st;
        if (count == 1)
            st = "st";
        else if (count == 2)
            st = "nd";
        else
            st = "th";
        vac_count_1 = message[0];
        vac_count_2 = message[1];

        if (!kill_process) {
            fprintf(stdout,
                    "Citizen %s (pid=%d) is vaccinated for the %d%s time: the clinic has %d vaccine1 and %d vaccine2.\n",
                    argv[2], getpid(), count, st, vac_count_1, vac_count_2);
            fflush(stdout);
        }
    }
    remaining = 0;
    for (int i = 0; i < citizen_array_size; ++i) {
        if (citizen_array[i].count != 0)
            remaining++;
    }

    if (!kill_process) {
        fprintf(stdout, "Citizen is leaving. Remaining citizens to vaccinate: %d\n", remaining);
        fflush(stdout);
    }

    munmap(citizen_array, sizeof(struct reg_citizen) * citizen_array_size);
    close(citizen_array_fd);
    sem_close(first_vaccine);
    sem_close(second_vaccine);
    sem_close(ready_to_go);
    close(my_fifo);
    unlink(fifoname);
    close(clinic);
    return 0;
}