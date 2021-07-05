#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include "data_types.h"


char* usage = "./program -n 2 -v 2 -c 3 -b 11 -t 3 -i inputfilepath\n"
              "n >= 2: the number of nurses (integer)\n"
              "v >= 2: the number of vaccinators (integer)\n"
              "c >= 3: the number of citizens (integer)\n"
              "b >= t * c + 1: size of the buffer (integer)\n"
              "t >= 1: how many times each citizen must receive the 2 shots (integer)\n"
              "i: pathname of the inputfile\n";

int terminate = 0;

/**
 * Signal Handler for ctrl + c signal
 * @param signum SIGINT
 */
void interrupt_handler(int signum) {
    if (signum == SIGINT)
        terminate = 1;
}

int main(int argc, char* argv[]) {
    int n_nurses = 0, n_vaccinators = 0, n_citizens = 0, buffer_size = 0, n_citizen_two_shots = 0;
    char* pathname = (char *) malloc(FILELENGTH * sizeof(char));

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("SIGACTION");
        exit(EXIT_FAILURE);
    }

    int opt;
    int n_flag = 0, v_flag = 0, c_flag = 0, b_flag = 0, t_flag = 0, i_flag = 0;
    while((opt = getopt(argc, argv, "n:v:c:b:t:i:")) != -1) {
        switch (opt) {
            case 'n':
                n_nurses = atoi(optarg);
                n_flag = 1;
                break;
            case 'v':
                n_vaccinators = atoi(optarg);
                v_flag = 1;
                break;
            case 'c':
                n_citizens = atoi(optarg);
                c_flag = 1;
                break;
            case 'b':
                buffer_size = atoi(optarg);
                b_flag = 1;
                break;
            case 't':
                n_citizen_two_shots = atoi(optarg);
                t_flag = 1;
                break;
            case 'i':
                strcpy(pathname, optarg);
                i_flag = 1;
                break;
            default:
                fprintf(stdout, "Unexpected argument!\n");
                fprintf(stdout, "%s", usage);
                exit(EXIT_FAILURE);
        }
    }
    if (!n_flag || !v_flag || !c_flag || !b_flag || !t_flag || !i_flag ||
        n_nurses < 2 || n_vaccinators < 2 || n_citizens < 3 ||
        buffer_size < n_citizen_two_shots * n_citizens + 1 || n_citizen_two_shots < 1) {
        fprintf(stdout, "Unexpected argument!\n");
        fprintf(stdout, "%s", usage);
        if (i_flag)
            free(pathname);
        exit(EXIT_FAILURE);
    }

    int file_descriptor = open(pathname, O_RDONLY);
    if (file_descriptor == -1) {
        perror("file_open");
        free(pathname);
        exit(EXIT_FAILURE);
    }

    /**
     * For the further use, create necessary semaphores and shared memory segments
     */
    pid_t pid_nurse[n_nurses];
    pid_t pid_citizen[n_citizens];
    pid_t pid_vaccinator[n_vaccinators];

    sem_t *full = sem_open("semaphore_1", O_CREAT, 0666, 0);
    sem_t *empty = sem_open("semaphore_2", O_CREAT, 0666, buffer_size);
    sem_t *mutex = sem_open("semaphore_3", O_CREAT, 0666, 1);
    sem_t *first_vaccine = sem_open("semaphore_4", O_CREAT, 0666, 0);
    sem_t *second_vaccine = sem_open("semaphore_5", O_CREAT, 0666, 0);
    sem_t *second_mutex = sem_open("semaphore_6", O_CREAT, 0666, 1);
    sem_t *ready_to_go = sem_open("semaphore_7", O_CREAT, 0666, 0);

    int shm_file_descriptor = shm_open("file_descriptor", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_file_descriptor, sizeof(int));
    int *shared_fd = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_file_descriptor, 0);
    memcpy(shared_fd, &file_descriptor, sizeof(int));

    int shared_buffer_fd = shm_open("shared_buffer", O_CREAT | O_RDWR, 0666);
    ftruncate(shared_buffer_fd, (buffer_size * sizeof(int)));
    int *buffer_array = mmap(NULL, (buffer_size * sizeof(int)), PROT_READ | PROT_WRITE, MAP_SHARED, shared_buffer_fd, 0);

    int shared_buffer_size_fd = shm_open("shared_buffer_size", O_CREAT | O_RDWR, 0666);
    ftruncate(shared_buffer_size_fd, sizeof(int));
    int *shared_buffer_size = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shared_buffer_size_fd, 0);
    memcpy(shared_buffer_size, &buffer_size, sizeof(int));

    int citizen_array_fd = shm_open("citizen_array", O_CREAT | O_RDWR, 0666);
    ftruncate(citizen_array_fd, (sizeof(struct reg_citizen) * n_citizens));
    struct reg_citizen *citizen_array = mmap(NULL, (sizeof(struct reg_citizen) * n_citizens), PROT_READ | PROT_WRITE, MAP_SHARED, citizen_array_fd, 0);

    int vaccine_types_fd = shm_open("vaccine_types", O_CREAT | O_RDWR, 0666);
    ftruncate(vaccine_types_fd, sizeof(int) * 3);
    int *vaccine_types = mmap(NULL, sizeof(int) * 3, PROT_READ |  PROT_WRITE, MAP_SHARED,  vaccine_types_fd, 0);


    mkfifo(clinic_fifo, 0666);

    fprintf(stdout, "Welcome to the GTU344 clinic. Number of citizen to vaccinate c=%d with t=%d doses\n", n_citizens, n_citizen_two_shots);
    fflush(stdout);

    /**
     * Fork and exec vaccinators
     */
    char total_vaccinator_count[INTEGERBITSIZE];
    sprintf(total_vaccinator_count, "%d", n_vaccinators);
    char citizen_count[INTEGERBITSIZE];
    sprintf(citizen_count, "%d", n_citizens);
    for (int i = 0; i < n_vaccinators; i++) {
        char vaccinator_count[INTEGERBITSIZE];
        sprintf(vaccinator_count, "%d", i);
        char *args[] = {"./vaccinator", vaccinator_count, citizen_count, total_vaccinator_count, NULL};
        pid_t temp = fork();
        switch (temp) {
            case -1:
                perror("FORK: ");
                break;
            case 0:
                execvp(args[0], args);
                _exit(EXIT_SUCCESS);
            default:
                pid_vaccinator[i] = temp;
                break;
        }
    }

    /**
     * Fork and exec nurses
     */
    for (int i = 0; i < n_nurses; i++) {
        char nurse_count[INTEGERBITSIZE];
        sprintf(nurse_count, "%d", i);
        char *args[] = {"./nurse", nurse_count, NULL};
        pid_t temp = fork();
        switch (temp) {
            case -1:
                perror("FORK: ");
                break;
            case 0:
                execvp(args[0], args);
                _exit(EXIT_SUCCESS);
            default:
                pid_nurse[i] = temp;
                break;
        }
    }

    /**
     * Fork and exec citizens
     */
    char how_many_shots[INTEGERBITSIZE];
    sprintf(how_many_shots, "%d", n_citizen_two_shots);
    char total_citizen[INTEGERBITSIZE];
    sprintf(total_citizen, "%d", n_citizens);
    for (int i = 0; i < n_citizens; ++i) {
        char citizenCount[INTEGERBITSIZE];
        sprintf(citizenCount, "%d", i);
        char *args[] = {"./citizen", how_many_shots, citizenCount, total_citizen, NULL};
        pid_t temp = fork();
        switch (temp) {
            case -1:
                perror("FORK: ");
                break;
            case 0:
                execvp(args[0], args);
                printf("EXEC ERROR");
                _exit(EXIT_SUCCESS);
            default:
                pid_citizen[i] = temp;
                char fifoname[INTEGERBITSIZE];
                sprintf(fifoname, "/tmp/%d", temp);
                if ((mkfifo(fifoname, 0666)) == -1)
                    perror("mkfifo: ");
                struct reg_citizen new_citizen;
                new_citizen.busy_flag = 0;
                new_citizen.count = n_citizen_two_shots;
                new_citizen.pid = temp;
                memcpy(citizen_array + i, &new_citizen, sizeof(struct reg_citizen));
                break;
        }
    }

    /**
     * After all the processes are created, every process should wait for every other process is ready for start.
     * So client waits for all children's message with a fifo
     * And then, it post ready_to_go semaphore and all of the processes start synchronously.
     */
    int total = n_citizens + n_vaccinators + n_nurses;
    int x = 0;
    int clinic_fd = open(clinic_fifo, O_RDONLY);
    while (x != total) {
        int message;
        read(clinic_fd, &message, sizeof(int));
        x++;
    }

    for (int i = 0; i < n_citizens; ++i) {
        sem_post(ready_to_go);
    }
    for (int i = 0; i < n_vaccinators; ++i) {
        sem_post(ready_to_go);
    }
    for (int i = 0; i < n_nurses; ++i) {
        sem_post(ready_to_go);
    }


    /**
     * Clinic waits for all the children finishes
     * While it's waiting, if a CTRL + C signal arrives, clinic sends a signal to all process
     * that they have to terminate immediately.
     */
    int wait_nurses = 0, flag_nurse = 0;
    int wait_vac = 0, flag_vac = 0;
    int wait_citizen = 0, flag_cit = 0;
    int vac_return[n_vaccinators];
    pid_t child_pid;
    while (!terminate) {
        int status;
        child_pid = waitpid(-1, &status, 0);
        if (terminate == 1) {
            fprintf(stdout, "\nCTRL + C Signal arrived...\n"
                            "All of the processes are terminating\n");
            fflush(stdout);
            for (int i = 0; i < n_nurses; ++i) {
                kill(pid_nurse[i], SIGINT);
            }
            for (int i = 0; i < n_vaccinators; ++i) {
                kill(pid_vaccinator[i], SIGINT);
            }
            for (int i = 0; i < n_citizens; ++i) {
                kill(pid_citizen[i], SIGINT);
            }
            break;
        }

        if (child_pid == -1)  {
            if (errno == ECHILD) {
                break;
            } else {
                perror("wait");
            }

        }
        for (int i = 0; i < n_nurses && wait_nurses != n_nurses; ++i) {
            if (pid_nurse[i] == child_pid) {
                wait_nurses++;
                break;
            }
        }
        for (int i = 0; i < n_vaccinators && wait_vac != n_vaccinators; i++) {
            if (pid_vaccinator[i] == child_pid) {
                wait_vac++;
                vac_return[i] = status / 256;
                break;
            }
        }
        for (int i = 0; i < n_citizens && wait_citizen != n_citizens; i++) {
            if (pid_citizen[i] == child_pid) {
                wait_citizen++;
                break;
            }
        }
        if (wait_nurses == n_nurses && !flag_nurse) {
            flag_nurse = 1;
            fprintf(stdout, "Nurses have carried all vaccines to the buffer, terminating.\n");
            fflush(stdout);
        }
        /**
         * If all of the citizen have finished, then, nurses and vacccinators should also be finished.
         * So clinic sends them a SIGUSR1 signal for them.
         */
        if (wait_citizen == n_citizens && !flag_cit) {
            flag_cit = 1;
            fprintf(stdout, "All citizens have been vaccinated.\n");
            fflush(stdout);
            for (int i = 0; i < n_vaccinators; ++i) {
                kill(pid_vaccinator[i], SIGUSR1);
            }
            for (int i = 0; i < n_nurses; ++i) {
                kill(pid_nurse[i], SIGUSR1);
            }
        }
        if (wait_vac == n_vaccinators && !flag_vac) {
            flag_vac = 1;
            for (int i = 0; i < n_vaccinators; ++i) {
                fprintf(stdout, "Vaccinator %d (pid=%d) vaccinated %d doses. ", i, pid_vaccinator[i], vac_return[i]);
                fflush(stdout);
            }
        }
    }
    if (!terminate) {
        fprintf(stdout, "The clinic is now closed. Stay Healthy.\n");
        fflush(stdout);
    }

    free(pathname);
    sem_close(full);
    sem_close(empty);
    sem_close(mutex);
    sem_close(first_vaccine);
    sem_close(second_vaccine);
    sem_close(second_mutex);
    sem_close(ready_to_go);
    munmap(shared_fd, sizeof(int));
    munmap(vaccine_types, sizeof(int) * 3);
    munmap(buffer_array, (buffer_size * sizeof(int)));
    munmap(shared_buffer_size, sizeof(int));
    munmap(citizen_array, (sizeof(struct reg_citizen) * n_citizens));
    close(shm_file_descriptor);
    close(shared_buffer_fd);
    close(shared_buffer_size_fd);
    close(citizen_array_fd);
    close(vaccine_types_fd);
    sem_unlink("semaphore_1");
    sem_unlink("semaphore_2");
    sem_unlink("semaphore_3");
    sem_unlink("semaphore_4");
    sem_unlink("semaphore_5");
    sem_unlink("semaphore_6");
    sem_unlink("semaphore_7");
    shm_unlink("file_descriptor");
    shm_unlink("shared_buffer");
    shm_unlink("shared_buffer_size");
    shm_unlink("citizen_array");
    shm_unlink("vaccine_types");

    unlink(clinic_fifo);
    
    return 0;
}