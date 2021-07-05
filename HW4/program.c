#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "queue.h"

#define MAX_STUDENT_SIZE 2048

char *usage = "./program homeworkFilePath studentsFilePath money";
char *h_thread_fifo_name = "/tmp/h_thread";
char *sem_student_name = "student_count";

struct student {
    char name[64];
    int quality;
    int speed;
    int price;
    int busy;
    int used;
    char filename[256];
    pthread_t pthread;
};

struct message {
    char homework;
    int money_left;
};

void *student_thread(void *arg);
void *h_thread_function(void *arg);

struct Queue *homework_queue;
int money;
int terminated = 0;

sem_t *student_count_semaphores;

void interrupt_handler (int signum) {
    if (signum == SIGINT)
        terminated = 1;
}

int main(int argc, char* argv[]) {
    char* homeworkPath;
    char* studentPath;

    struct student structstudent[MAX_STUDENT_SIZE];

    if (argc != 4) {
        fprintf(stdout, "Invalid Arguments!\n");
        fprintf(stdout, "Usage: %s\n", usage);
        return -1;
    }
    homeworkPath = argv[1];
    studentPath = argv[2];
    money = atoi(argv[3]);

    int student_count = 0;

    if (money == 0) {
        fprintf(stdout, "Invalid Arguments!\n");
        fprintf(stdout, "Usage: %s\n", usage);
        return -1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("SIGACTION");
        exit(EXIT_FAILURE);
    }


    int f_student = open(studentPath, O_RDONLY);
    char t_name[64];
    int t_quality, t_speed, t_price;
    char temp[1024];
    int i = 0;
    while (i != 1024) {
        int size = read(f_student, &temp[i++], 1);
        if (size == 0 || size == -1) {
            temp[i - 1] = '\0';
            sscanf(temp, "%s %d %d %d", t_name, &t_quality, &t_speed, &t_price);
            struct student s_temp;
            s_temp.speed = t_speed;
            s_temp.price = t_price;
            s_temp.busy = 0;
            s_temp.quality = t_quality;
            s_temp.used = 0;
            char fifoname[256];
            sprintf(fifoname, "/tmp/thread_fifo_%d", student_count);
            strcpy(s_temp.filename,fifoname);
            strcpy(s_temp.name, t_name);
            structstudent[student_count++] = s_temp;
            i = 0;
            break;
        }
        if (temp[i - 1] == '\n') {
            temp[i - 1] = '\0';
            sscanf(temp, "%s %d %d %d", t_name, &t_quality, &t_speed, &t_price);
            struct student s_temp;
            s_temp.speed = t_speed;
            s_temp.price = t_price;
            s_temp.busy = 0;
            s_temp.quality = t_quality;
            s_temp.used = 0;
            char fifoname[256];
            sprintf(fifoname, "/tmp/thread_fifo_%d", student_count);
            strcpy(s_temp.filename,fifoname);
            strcpy(s_temp.name, t_name);
            structstudent[student_count++] = s_temp;
            i = 0;
        }
    }

    fprintf(stdout, "%d students-for-hire threads have been created\n", student_count);
    fprintf(stdout, "Name Q S C\n");

    int error_flag = 0;
    for (int j = 0; j < student_count; j++) {
        fprintf(stdout, "%s %d %d %d\n", structstudent[j].name, structstudent[j].quality, structstudent[j].speed, structstudent[j].price);
        if(mkfifo(structstudent[j].filename, 0666) == -1) {
            perror("mkfifo: structstudent filename :");
            for (int k = 0; k < student_count; k++) {
                unlink(structstudent[k].filename);
            }
            error_flag = 1;
        }
    }
    if (error_flag == 1) {
        printf("EXIT");
        exit(EXIT_FAILURE);
    }


    for (int j = 0; j < student_count; j++) {
        pthread_create(&structstudent[j].pthread, NULL, student_thread, &structstudent[j]);
    }

    student_count_semaphores = sem_open(sem_student_name, O_CREAT, 0666, student_count - 1);

    if ((mkfifo(h_thread_fifo_name, 0666)) == -1) {
        perror("mkfifo");
        unlink(h_thread_fifo_name);
        for (int k = 0; k < student_count; k++) {
            unlink(structstudent[k].filename);
        }
        exit(EXIT_FAILURE);
    }

    pthread_t h_thread;
    pthread_attr_t attr;
    int s = pthread_attr_init(&attr);
    if (s != 0) {
        perror("pthread_attr_init(&attr)");
        unlink(h_thread_fifo_name);
        for (int k = 0; k < student_count; k++) {
            unlink(structstudent[k].filename);
        }
        exit(EXIT_FAILURE);
    }
    s = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (s != 0) {
        perror("pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)");
        pthread_attr_destroy(&attr);
        unlink(h_thread_fifo_name);
        for (int k = 0; k < student_count; k++) {
            unlink(structstudent[k].filename);
        }
        exit(EXIT_FAILURE);
    }

    s = pthread_create(&h_thread, NULL, h_thread_function, homeworkPath);
    int fifo_fd = open(h_thread_fifo_name,  O_RDONLY);

    if (s != 0) {
        perror("pthread_create(&h, &attr, h_thread, homeworkPath)");
        pthread_attr_destroy(&attr);
        unlink(h_thread_fifo_name);
        for (int k = 0; k < student_count; k++) {
            unlink(structstudent[k].filename);
        }
        exit(EXIT_FAILURE);
    }

    s = pthread_attr_destroy(&attr);
    if (s != 0) {
        perror("pthread_attr_destroy(&attr)");
        pthread_attr_destroy(&attr);
        unlink(h_thread_fifo_name);
        for (int k = 0; k < student_count; k++) {
            unlink(structstudent[k].filename);
        }
        exit(EXIT_FAILURE);
    }

    int student_fd[student_count];

    for (int j = 0; j < student_count; j++) {
        student_fd[j] = open(structstudent[j].filename, O_WRONLY);
    }

    char dummy = 'X';

    int flag = 0;
    while(money > 0 && dummy != 'E' && terminated == 0) {
        flag = 0;
        for (int j = 0; j < student_count; j++) {
            if (structstudent[i].price < money) {
                flag = 1;
                break;
            }
        }
        if (flag == 0)
            break;

        read(fifo_fd, &dummy, sizeof(char));
        if (terminated == 1)
            break;

        sem_wait(student_count_semaphores);

        if (terminated == 1)
            break;
        if (dummy == 'E') {
            fprintf(stdout, "No more homeworks left or coming in, closing.\n");
            break;
        } else {
            char homework = dequeue(homework_queue);
            int max = 0;
            if (homework == 'Q') {
                for (int j = 1; j < student_count; j++) {
                    if (structstudent[j].quality > structstudent[j - 1].quality && structstudent[j].busy != 1
                                                                                && money >= structstudent[j].price)
                        max = j;
                }
            } else if (homework == 'S') {
                for (int j = 1; j < student_count; j++) {
                    if (structstudent[j].speed > structstudent[j - 1].speed && structstudent[j].busy != 1
                                                                                && money >= structstudent[j].price)
                        max = j;
                }
            } else if (homework == 'C') {
                for (int j = 1; j < student_count; j++) {
                    if (structstudent[j].price < structstudent[j - 1].price && structstudent[j].busy != 1
                                                                                && money >= structstudent[j].price)
                        max = j;
                }
            }
            if (structstudent[max].price < money) {
                money = money - structstudent[max].price;
                structstudent[max].busy = 1;
                write(student_fd[max], &homework, sizeof(char));
                write(student_fd[max], &money, sizeof(int));
            } else {
                flag = 0;
                break;
            }
        }
    }

    if (terminated == 1)  {
        fprintf(stdout, "\nTermination signal received, closing.\n");
        for (int j = 0; j < student_count; j++) {
            pthread_kill(structstudent[j].pthread, SIGINT);
        }
    }
    else {
        char exit_msg = 'E';
        int exit_int = 0;
        for (int j = 0; j < student_count; j++) {
            write(student_fd[j], &exit_msg, sizeof(char));
            write(student_fd[j], &exit_int, sizeof(int));
        }

        for (int j = 0; j < student_count; j++) {
            pthread_join(structstudent[j].pthread, NULL);
        }


        if ((money == 0 || flag == 0) && !terminated) {
            fprintf(stdout, "Money is over, closing.\n");
        }

        if (terminated == 0) {
            fprintf(stdout, "Homeworks solved and money made by students\n");
            fprintf(stdout, "Name Q S C\n");
            for (int j = 0; j < student_count; j++) {
                if (structstudent[j].used != 0) {
                    fprintf(stdout, "%s %d %d %d\n", structstudent[j].name, structstudent[j].quality, structstudent[j].speed,
                            (structstudent[j].used * structstudent[j].price));
                }
            }
            fprintf(stdout, "Money left at G's account: %dTL\n", money);
        }

    }

    pthread_join(h_thread, NULL);

    for (int j = 0; j < student_count; j++) {
        close(student_fd[j]);
        unlink(structstudent[j].filename);
    }

    free(homework_queue->array);
    free(homework_queue);
    close(fifo_fd);
    unlink(h_thread_fifo_name);
    sem_close(student_count_semaphores);
    sem_unlink(sem_student_name);

    return 0;
}

void *h_thread_function(void *arg) {
    char* homeworkPath = (char*) arg;
    int hw_fd = open(homeworkPath, O_RDONLY);
    int fifo = open(h_thread_fifo_name, O_WRONLY);

    char dummy = 'X';
    int file_size = 0;
    while (terminated == 0) {
        char buffer;
        int size = read(hw_fd, &buffer, 1);
        if (size == 0 || size == -1) {
            break;
        }
        file_size++;
    }

    homework_queue = createQueue(file_size);

    lseek(hw_fd, 0, SEEK_SET);
    while (money != 0 && terminated == 0) {
        char buffer;
        int size = read(hw_fd, &buffer, 1);
        if (size == 0 || size == -1 || terminated  == 1) {
            break;
        }
        enqueue(homework_queue, buffer);
        write(fifo, &dummy, sizeof(char));
    }
    dummy = 'E';
    write(fifo, &dummy, sizeof(char));

    close(fifo);
    pthread_exit(0);
}

void *student_thread(void *arg) {
    struct student *my_info = (struct student*) arg;
    int fd = open(my_info->filename, O_RDONLY);
    if (fd == -1) {
        perror("open:");
        pthread_exit(0);
    }
    char input_homework = 'X';
    int input_money;
    while (input_homework != 'E' && terminated == 0) {
        fprintf(stdout, "%s is waiting for a homework\n", my_info->name);
        read(fd, &input_homework, sizeof(char));
        my_info->busy = 1;
        if (input_homework == 'E' || terminated == 1)
            break;
        read(fd, &input_money, sizeof(int));
        fprintf(stdout, "%s is solving homework %c for %d, H has %d left.\n", my_info->name, input_homework, my_info->price, input_money);
        sleep(6 - my_info->speed);
        if (terminated == 1)
            break;
        my_info->busy = 0;
        my_info->used = my_info->used + 1;
        sem_post(student_count_semaphores);
    }

    close(fd);
    pthread_exit(0);
}