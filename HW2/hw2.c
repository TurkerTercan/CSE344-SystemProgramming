#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#define BUFFER_SIZE 2048

int main_count = 0;
int terminated = 0;
int file_descriptor;

void child_process(int row, char* buffer);
double interpolation(double x[], double y[], int n, double xp);
void calculate_absolute_error(int descriptor, int degree);
void write_to_file(char buf[], struct flock lock, int increase_size, char* buffer, int row);
void lagrange(double x[], double y[], int n, int row);
/**
 * Catcher functions
 * @param signum
 */
void M_catcher(int signum) {
    main_count++;

    if (signum == SIGTERM){
        terminated = 1;
    }

}

void child_catcher(int signum) {
    if (signum == SIGTERM){
        terminated = 1;
    }
}


int main(int argc, char *argv[]) {
    char buffers[8][BUFFER_SIZE];
    pid_t children[8];

    /**
    * Usage information
    */
    if (argc != 2) {
        perror("Error:\tUsage: ./program filePath");
        return 1;
    }

    /**
     * I opened the file with both read and write permission
     * because it is easier when children process wants do it both
     */
    file_descriptor = open(argv[1], O_RDWR, 0666);
    if (file_descriptor == -1) {
        perror("Open");
        return 1;
    }

    /**
     * Defining signal handlers for parent process
     */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &M_catcher;
    act.sa_flags = SA_NODEFER | SA_RESTART;


    if (sigaction(SIGUSR1, &act, NULL) == -1 ||
        sigaction(SIGTERM, &act, NULL) == -1)
    {
        perror("Main Sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGUSR1);
    sigdelset(&mask, SIGTERM);

    if (sigprocmask(SIG_SETMASK, &mask, NULL) < 0) {
        perror("Main sigprocmask() SIGUSR1");
        exit(EXIT_FAILURE);
    }


    for (int i = 0; i < 8; i++) {
        children[i] = fork();
        switch (children[i]) {
            case -1:
                perror("Fork");
                break;
            case 0:
                child_process(i, buffers[i]);
                _exit(EXIT_SUCCESS);
            default:
                break;
        }
    }

    /**
     * After fork wait 8 children process to finish their job
     */
    while (main_count < 8) {
        sigsuspend(&mask);
        if (terminated == 1) {
            printf("Program terminated");
            close(file_descriptor);
            for (int j = 0; j < 8; ++j) {
                kill(children[j], SIGTERM);
            }
            exit(EXIT_FAILURE);
        }
    }

    sigemptyset(&mask);


    if (sigprocmask(SIG_SETMASK, &mask, NULL) < 0) {
        perror("Main sigprocmask() SIGUSR1");
        exit(EXIT_FAILURE);
    }

    if (terminated == 1) {
        printf("Program terminated");
        close(file_descriptor);
        for (int j = 0; j < 8; ++j) {
            kill(children[j], SIGTERM);
        }
        exit(EXIT_FAILURE);
    }


    calculate_absolute_error(file_descriptor, 5);

    for (int j = 0; j < 8; ++j) {
        kill(children[j], SIGUSR1);
    }

    main_count = 0;

    sigfillset(&mask);
    sigdelset(&mask, SIGUSR1);
    sigdelset(&mask, SIGTERM);

    if (sigprocmask(SIG_SETMASK, &mask, NULL) < 0) {
        perror("Main sigprocmask() SIGUSR1");
        exit(EXIT_FAILURE);
    }

    /**
     * After fork wait 8 children process to finish their job
     */
    while (main_count < 8) {
        sigsuspend(&mask);
        if (terminated == 1) {
            printf("Program terminated");
            close(file_descriptor);
            for (int j = 0; j < 8; ++j) {
                kill(children[j], SIGTERM);
            }
            exit(EXIT_FAILURE);
        }
    }

    sigemptyset(&mask);

    calculate_absolute_error(file_descriptor, 6);


    pid_t child_pid;
    while (1) {
        child_pid = wait(NULL);
        if (child_pid == -1) {
            if (errno == ECHILD) {
                close(file_descriptor);
                break;
            } else {
                perror("wait");
            }
        }
    }

    close(file_descriptor);
    exit(EXIT_SUCCESS);
}

void child_process(int row, char* buffer) {
    double x[8], y[8];
    struct flock lock;
    int disable = 0;
    int increase_size = 0;

    while (disable == 0){
        lseek(file_descriptor, 0, SEEK_SET);
        int size = read(file_descriptor, buffer, BUFFER_SIZE);

        buffer[size] = '\0';
        int k = 0;
        increase_size = 0;
        char *start = buffer;

        while (k != row && increase_size < size) {
            if (*buffer == '\n')
                k++;
            buffer++;
            increase_size++;
        }
        char *temp = buffer;

        k = 0;
        while (k != 8) {
            x[k] = atof(temp);
            if (x[k] != 0)
                disable = 1;
            while (*temp != ',' && temp < start + size)
                temp++;
            if (temp < start + size)
                temp++;
            y[k] = atof(temp);
            if (y[k] != 0)
                disable = 1;
            if (k != 7) {
                while (*temp != ',' && temp < start + size)
                    temp++;
                if (temp < start + size)
                    temp++;
            }
            k++;
        }
    }

    double result = interpolation(x, y, 5, x[7]);
    char buf[BUFFER_SIZE];
    if (isnan(result)){
        result = -1.1;
    }
    sprintf(buf, ",%.1f", result);

    write_to_file(buf, lock, increase_size, buffer, row);

    kill(getppid(), SIGUSR1);


    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGUSR1);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &child_catcher;
    act.sa_flags = SA_NODEFER;

    if (sigaction(SIGTERM, &act, NULL) < 0 ||
        sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("Sigaction child SIGTERM or SIGUSR1");
        exit(EXIT_FAILURE);
    }

    if (sigprocmask(SIG_SETMASK, &set, NULL) < 0) {
        perror("child process sigprocmask()");
        exit(EXIT_FAILURE);
    }

    kill(getppid(), SIGUSR1);
    sigsuspend(&set);

    if (terminated == 1) {
        printf("Program terminated\n");
        close(file_descriptor);
        return;
    }

    result = interpolation(x, y, 6, x[7]);
    if (isnan(result)){
        result = -1.1;
    }
    sprintf(buf, ",%.1f", result);

    write_to_file(buf, lock, increase_size, buffer, row);
    kill(getppid(), SIGUSR1);

    lagrange(x, y, 7, row);

    kill(getppid(), SIGUSR1);
    close(file_descriptor);
    return;
}

double interpolation(double x[], double y[], int n, double xp) {
   double polation = 0;
   double L[n];

    for (int i = 0; i < n; ++i) {
        double result = 1;
        for (int j = 0; j < n; ++j) {
            if (i != j)
                result = result * (double) ((xp - x[j]) / (x[i] - x[j]));
        }
        L[i] = result;
        polation = polation + (y[i] * L[i]);
    }
    return polation;
}

void write_to_file(char buf[], struct flock lock, int increase_size, char* buffer, int row) {
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    fcntl(file_descriptor, F_SETLKW, &lock);

    if (row == 7) {
        lseek(file_descriptor, -1, SEEK_END);
        char ch;
        read(file_descriptor, &ch, 1);
        if (ch == '\n')
            lseek(file_descriptor, -1, SEEK_END);
        else
            lseek(file_descriptor, 0, SEEK_END);

        write(file_descriptor, buf, strlen(buf));
    }

    else {
        buffer -= increase_size;
        lseek(file_descriptor, 0, SEEK_SET);
        int size = read(file_descriptor, buffer, BUFFER_SIZE);
        int temp_count = size;
        int char_count = 0;
        int row_count = 0;
        while (row_count != row + 1) {
            while (buffer[char_count] != '\n')
                char_count++;
            row_count++;
            char_count++;
        }
        while (size  > char_count - 1) {
            buffer[size + strlen(buf)] = buffer[size];
            size--;
        }

        for (int j = 0; j < (int) strlen(buf); j++) {
            buffer[j + char_count - 1] = buf[j];
        }
        buffer[strlen(buf) + char_count - 1] = '\n';
        lseek(file_descriptor, 0, SEEK_SET);
        write(file_descriptor, buffer, temp_count + strlen(buf));
    }

    lock.l_type = F_UNLCK;
    fcntl(file_descriptor, F_SETLKW, &lock);
}

void calculate_absolute_error(int descriptor, int degree) {
    char* buffer = (char*) malloc(BUFFER_SIZE * sizeof(char));
    char* temp = buffer;
    lseek(file_descriptor, 0, SEEK_SET);
    int size = read(descriptor, buffer, BUFFER_SIZE);

    double error_size = 0;
    double a, b;

    int i = 0, j = 0, k = 0;

    int a_flag = 0;
    int b_flag = 0;
    while (k < size) {
        if (j == 15 && a_flag == 0) {
            int temp = 0;
            char *temp_str;
            while (buffer[temp] != ',' && ((temp + k ) < size - 1))
                temp++;
            temp_str = (char *) malloc((temp + 2) * sizeof(char));
            strncpy(temp_str, buffer, temp);
            temp_str[temp + 1] = '\0';
            temp_str[temp] = '\n';
            a = atof(temp_str);
            a_flag = 1;
            free(temp_str);
        }
        if ( (j == 16 && degree == 5 && b_flag == 0) || (j == 17 && degree == 6 && b_flag == 0)) {
            int temp = 0;
            char *temp_str;
            while (buffer[temp] != ',' && ((temp + k ) < size - 1))
                temp++;
            temp_str = (char *) malloc((temp + 2) * sizeof(char));
            strncpy(temp_str, buffer, temp);
            temp_str[temp + 1] = '\0';
            temp_str[temp] = '\n';
            b = atof(temp_str);
            b_flag = 1;

            error_size += abs(a - b);
            free(temp_str);
        }


        if (*buffer != '\n' && *buffer != ','){
            buffer++;
            k++;
        }
        else if (*buffer == '\n'){
            buffer++;
            i++;
            k++;
            j = 0;
            a_flag = 0;
            b_flag = 0;
        } else if (*buffer == ',') {
            buffer++;
            k++;
            j++;
        }

    }

    fprintf(stdout, "Error of polynomial of degree %d: %.1f\n", degree, error_size);
    free(temp);
}

void lagrange(double x[], double y[], int n, int row) {

    double coefficients[7];
    for (int m = 0; m < 7; ++m) {
        coefficients[m] = 0;
    }

    for (int m = 0; m < 7; ++m) {
        double new_coefficients[7];
        for (int n = 0; n < 7; ++n) {
            new_coefficients[n] = 0;
        }
        if (m > 0) {
            new_coefficients[0] = (-1 * x[0]) / (x[m] - x[0]);
            new_coefficients[1] = 1 / (x[m]  - x[0]);
        } else {
            new_coefficients[0] = (-1 * x[1]) / (double) (x[m] - x[1]);
            new_coefficients[1] = 1 /(x[m] - x[1]);
        }
        int index = 1;
        if (m == 0)
            index = 2;
        for (int k = index; k < 7; ++k) {
            if (m == k)
                continue;
            for (int nc = 6; nc >= 1 ; --nc) {
                new_coefficients[nc] = new_coefficients[nc] * ((-1 * x[k]) /(double) (x[m] - x[k])) +
                        new_coefficients[nc - 1] /(double) (x[m] - x[k]);
            }
            new_coefficients[0] = new_coefficients[0] * ((-1  * x[k]) / (x[m] - x[k]));
        }
        for (int nc = 0; nc < 7; ++nc) {
            coefficients[nc] += y[m] * new_coefficients[nc];

        }
    }

    printf("Polynomial %d: ", row);
    for (int i = 0; i < 7; ++i) {
        if (!isnan(coefficients[i])) {
            printf("%.1f,", coefficients[i]);
        }
    }
    printf("\n");
}
