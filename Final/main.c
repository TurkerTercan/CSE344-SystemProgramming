#include "server.h"
#include "Database.h"

int terminated = false;
int available_pools;
sem_t *server_sem;
int logFileFD, datasetFD;
char logstr[5000];

void interrupt_handler(int signum) {
    if (signum == SIGINT) {
        terminated = true;
    }
}

pthread_mutex_t mutex_socket;
pthread_cond_t main_cond;
struct Database *database;
int active_readers = 0;
int active_writers = 0;
int waiting_readers = 0;
int waiting_writers = 0;
pthread_cond_t condRead;
pthread_cond_t condWrite;
pthread_mutex_t lockThread;

struct Database *update_database(struct Database *pDatabase, char str[2048], struct pool_t *pPool);
struct Database *select_database(struct Database *pDatabase, char str[2048], struct pool_t *pPool);

void *pool_thread(void* arg);

int main(int argc, char* argv[]) {
    char *usage = "Invalid Arguments!\nExample Usage: ./server -p PORT -o pathToLogFile –l poolSize –d datasetPath\n";
    int port, poolsize;
    char pathToLogFile[2000], datasetPath[2000];
    int opt;


    int p_flag = 0, o_flag = 0, l_flag = 0, d_flag = 0;
    while ((opt = getopt(argc, argv, "p:o:l:d:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                p_flag = 1;
                break;
            case 'o':
                strcpy(pathToLogFile, optarg);
                o_flag = 1;
                break;
            case 'l':
                poolsize = atoi(optarg);
                l_flag = 1;
                break;
            case 'd':
                strcpy(datasetPath, optarg);
                d_flag = 1;
                break;
            default:
                fprintf(stdout, "%s", usage);
                exit(EXIT_FAILURE);
        }
    }

    if (o_flag == 0 || p_flag == 0 || l_flag == 0 || d_flag == 0) {
        fprintf(stdout, "%s", usage);
        exit(EXIT_FAILURE);
    }
    server_sem = sem_open("server_sem", O_CREAT | O_EXCL, 0666);

    if (server_sem == NULL) {
        fprintf(stdout, "Already one server instance is running.\nClosing...\n");
        exit(EXIT_FAILURE);
    }
    sem_close(server_sem);

    becomeDaemon(01);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        char* log = "SIGACTION SIGINT";
        write(logFileFD, log, strlen(log));
        exit(EXIT_FAILURE);
    }


    logFileFD = open(pathToLogFile, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    datasetFD = open(datasetPath, O_RDONLY, 0777);

    sprintf(logstr, "Executing with parameters:\n"
                    "-p %d\n"
                    "-o %s\n"
                    "-l %d\n"
                    "-d %s\n", port, pathToLogFile, poolsize, datasetPath);
    write(logFileFD, logstr, strlen(logstr));


    database = parse_csv(datasetFD, logFileFD);

    //Create Socket for server connection
    int socket_fd = 0, connection_fd = 0, c;
    struct sockaddr_in server, client;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        sprintf(logstr, "Error: Socket Create\n");
        write(logFileFD, logstr, strlen(logstr));
        close(socket_fd);
        close_database(database);
        sem_close(server_sem);
        sem_unlink("server_sem");
        exit(EXIT_FAILURE);
    }
    int x = true;
    if (setsockopt(socket_fd,SOL_SOCKET, SO_REUSEADDR, &x, sizeof(int)) == -1) {
        sprintf(logstr, "Error: Setsockopt\n");
        write(logFileFD, logstr, strlen(logstr));
        close(socket_fd);
        close_database(database);
        sem_close(server_sem);
        sem_unlink("server_sem");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    int retVal = bind(socket_fd, (struct sockaddr*)&server, sizeof(server));
    if (retVal < 0) {
        sprintf(logstr, "Error: Socket Bind\n");
        write(logFileFD, logstr, strlen(logstr));
        close(socket_fd);
        close_database(database);
        sem_close(server_sem);
        sem_unlink("server_sem");
        exit(EXIT_FAILURE);
    }
    retVal = listen(socket_fd, 1024);
    if (retVal != 0) {
        sprintf(logstr, "Error: Socket Listen\n");
        write(logFileFD, logstr, strlen(logstr));
        close(socket_fd);
        close_database(database);
        sem_close(server_sem);
        sem_unlink("server_sem");
        exit(EXIT_FAILURE);
    }

    available_pools = poolsize;
    pthread_mutex_init(&mutex_socket, NULL);
    pthread_mutex_init(&lockThread, NULL);
    pthread_cond_init(&main_cond, NULL);
    pthread_cond_init(&condRead, NULL);
    pthread_cond_init(&condWrite, NULL);

    struct pool_t pool_threads[poolsize];
    for (int i = 0; i < poolsize; i++) {
        pool_threads[i].busy_flag = 0;
        pool_threads[i].id = i;
        pool_threads[i].connection_socket_fd = -1;
        pthread_cond_init(&pool_threads[i].cond, NULL);
        pthread_create(&pool_threads[i].pool_thread, NULL, pool_thread, &pool_threads[i]);
    }

    sprintf(logstr, "A pool of %d threads has been created\n", poolsize);
    write(logFileFD, logstr, strlen(logstr));
    c = sizeof(struct sockaddr_in);

    while (terminated == false) {
        connection_fd = accept(socket_fd, (struct sockaddr*) &client, (socklen_t*)&c);
        if (terminated == true) {
            break;
        }
        pthread_mutex_lock(&mutex_socket);
        while (available_pools == 0) {
            sprintf(logstr, "No thread is available! Waiting...\n");
            write(logFileFD, logstr, strlen(logstr));
            pthread_cond_wait(&main_cond, &mutex_socket);
        }
        int selected;
        for (selected = 0; selected < poolsize; selected++) {
            if (pool_threads[selected].busy_flag == 0 && pool_threads[selected].connection_socket_fd == -1) {
                pool_threads[selected].busy_flag = 1;
                pool_threads[selected].connection_socket_fd = connection_fd;

                sprintf(logstr, "A connection has been delegated to thread id #%d\n", selected);
                write(logFileFD, logstr, strlen(logstr));

                pthread_cond_signal(&pool_threads[selected].cond);
                available_pools--;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_socket);
    }

    if (terminated == true) {
        sprintf(logstr, "Termination signal received, waiting for ongoing threads to complete.\n");
        write(logFileFD, logstr, strlen(logstr));

        for (int i = 0; i < poolsize; i++) {
            pthread_cond_signal(&pool_threads[i].cond);
            pthread_join(pool_threads[i].pool_thread, NULL);
        }

        sprintf(logstr, "All threads have terminated, server shutting down.\n");
        write(logFileFD, logstr, strlen(logstr));
    }

    for (int i = 0; i < poolsize; i++) {
        pthread_cond_destroy(&pool_threads[i].cond);
    }
    pthread_mutex_destroy(&mutex_socket);
    pthread_mutex_destroy(&lockThread);
    pthread_cond_destroy(&main_cond);
    pthread_cond_destroy(&condRead);
    pthread_cond_destroy(&condWrite);
    shutdown(socket_fd, SHUT_RDWR);
    close(connection_fd);
    close(socket_fd);
    close_database(database);
    sem_unlink("server_sem");
    return 0;
}



void *pool_thread(void* arg) {
    struct pool_t* thread_info = (struct pool_t*) arg;
    struct timespec ts = {0, 500000000L};
    char thread_log[3000] = "";
    char receiver_buf[2048] = "";

    while (terminated == false) {
        pthread_mutex_lock(&mutex_socket);
        if (thread_info->busy_flag == 0 || thread_info->connection_socket_fd == -1) {
            sprintf(thread_log, "Thread #%d: waiting for connection\n", thread_info->id);
            write(logFileFD, thread_log, strlen(thread_log));
            pthread_cond_wait(&thread_info->cond, &mutex_socket);
        }
        pthread_mutex_unlock(&mutex_socket);
        if (terminated == true) {
            pthread_exit(0);
        }

        int size;
        int query_size;
        int loop_counter = 0;
        read(thread_info->connection_socket_fd, &query_size, sizeof(int));
        while (loop_counter < query_size) {
            size = read(thread_info->connection_socket_fd, receiver_buf, sizeof(receiver_buf));
            receiver_buf[size] = '\0';
            sprintf(thread_log, "Thread #%d: received query '%s'\n", thread_info->id, receiver_buf);
            write(logFileFD, thread_log, strlen(thread_log));

            struct Database* returnTable;
            if (receiver_buf[0] == 'U')
                returnTable = update_database(database, receiver_buf, thread_info);
            if (receiver_buf[0] == 'S')
                returnTable = select_database(database, receiver_buf, thread_info);

            unsigned int count = 0;

            struct row_node* row = returnTable->first_row;
            for (int i = 0; i < returnTable->row_size; i++) {
                for (int j = 0; j < returnTable->column_size; j++) {
                    count = count + strlen(row->string[j]) + 3;
                }
                row = row->next_row;
            }
            char *output_str = (char*) malloc(sizeof(char) * count);
            output_str[0] = ' ';
            output_str[count - 1] = '\0';
            char *current = output_str;
            row = returnTable->first_row;
            for (int i = 0; i < returnTable->row_size; i++) {
                for (int j = 0; j < returnTable->column_size; j++) {
                    if (j == returnTable->column_size - 1)
                        sprintf(current, "%s\n", row->string[j]);
                    else
                        sprintf(current, "%s\t", row->string[j]);
                    current = current + strlen(row->string[j]) + 1;
                }
                if (i == returnTable->row_size - 1) {
                    current[0] = 4;
                    current[1] = '\0';
                }

                row = row->next_row;
            }

            nanosleep(&ts, NULL);
            write(thread_info->connection_socket_fd, &returnTable->row_size, sizeof(int));
            write(thread_info->connection_socket_fd, output_str, strlen(output_str) * sizeof(char));

            free(output_str);
            sprintf(thread_log, "Thread #%d: query completed, %d records have been returned\n", thread_info->id, (returnTable->row_size + 1));
            write(logFileFD, thread_log, strlen(thread_log));
            if (returnTable != database)
                close_database(returnTable);
            loop_counter++;
        }
        close(thread_info->connection_socket_fd);
        thread_info->connection_socket_fd = -1;
        thread_info->busy_flag = 0;
        available_pools++;
        pthread_cond_signal(&main_cond);
    }

    pthread_exit(0);
}

struct Database *select_database(struct Database *pDatabase, char str[2048], struct pool_t *pPool) {
    //PARSE SQL
    int distinct_flag = 0;
    int* column_id;
    int condition_id = -1;
    int column_size = 0;
    char* condition_rule = NULL;
    char input[2048];
    char slog[2148];
    strcpy(input, str);

    char* current = str;
    unsigned int size = strlen(str);
    int c = 0;
    int comma = 0;
    int flag = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] == ' ' && flag == 0) {
            str[i] = '\0';
            c++;
        }
        if (str[i] == ',' && flag == 0) {
            comma = comma + 1;
        }
        if (str[i] == ';') {
            c++;
        }
        if (str[i] == '\'' && flag == 0) {
            flag = 1;
            str[i] = '"';
        }
        else if (str[i] == '\'' && flag == 1) {
            flag = 0;
            str[i] = '"';
        }
    }
    current = str;

    if ((strcmp(str, "SELECT") != 0) || c < 4) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    current = current + strlen(current) + 1;
    if (strcmp(current, "DISTINCT") == 0) {
        distinct_flag = 1;
        current = current + strlen(current) + 1;
    }
    if (strcmp(current, "*") == 0) {
        column_id = (int*) malloc(pDatabase->column_size * sizeof(int));
        column_size = pDatabase->column_size;
        for (int i = 0; i < pDatabase->column_size; i++) {
            column_id[i] = i;
        }
        current = current + strlen(current) + 1;
    }
    else {
        column_id = (int*) malloc(sizeof(int) * (comma + 1));
        column_size = comma + 1;
        struct row_node* identifiers = pDatabase->first_row;
        for (int i = 0; i < comma + 1; i++) {
            int cflag = 0;
            if (current[strlen(current) - 1] == ',') {
                cflag = 1;
                current[strlen(current) - 1] = '\0';
            }
            int found = 0;
            for (int j = 0; j < pDatabase->column_size; j++) {
                char* temp = (char*) malloc(sizeof(char) * (strlen(identifiers->string[j]) + 3));
                sprintf(temp, "\"%s\"", identifiers->string[j]);
                temp[strlen(identifiers->string[j]) + 2] = '\0';
                if ((strcmp(current, identifiers->string[j]) == 0) || (strcmp(current, temp) == 0)) {
                    column_id[i] = j;
                    found = 1;
                    free(temp);
                    break;
                }
                free(temp);
            }
            if (found == 0) {
                sprintf(slog, "Error 1: could not prepare statement (1 no such column: %s)\n", current);
                write(logFileFD, slog, strlen(slog) * sizeof(char));

                char** empty_str = (char**) malloc(sizeof(char*) * 1);
                char* temp = "empty database";
                empty_str[0] = temp;
                struct Database* empty = init_database(1, empty_str);
                free(empty_str);
                return empty;
            }
            if (cflag == 0) {
                current = current + strlen(current) + 1;
            } else {
                current = current + strlen(current) + 2;
            }
        }
    }
    if (strcmp(current, "FROM") != 0) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    current = current + strlen(current) + 1;
    if (strcmp(current, "TABLE") == 0 ) {
        current = current + strlen(current) + 1;
        if (strcmp(current, "WHERE") == 0) {
            current = current + strlen(current) + 1;
            char* t = current;
            unsigned int x = strlen(current);
            int e_flag = 0;
            int c_flag = 0;
            for (int i = 0; i < x; i++) {
                if (t[i] == '=' && e_flag == 0) {
                    t[i] = '\0';
                    e_flag = 1;
                }
                if (t[i] == ';' && c_flag == 0) {
                    t[i] = '\0';
                    c_flag = 1;
                }
            }
            if (e_flag == 0 || c_flag == 0) {
                sprintf(slog, "Invalid Query: '%s'\n", input);
                write(logFileFD, slog, strlen(slog) * sizeof(char));
                char** empty_str = (char**) malloc(sizeof(char*) * 1);
                char* temp = "empty database";
                empty_str[0] = temp;
                struct Database* empty = init_database(1, empty_str);
                free(empty_str);
                return empty;
            }
            struct row_node* identifiers = pDatabase->first_row;
            int found = 0;
            for (int j = 0; j < pDatabase->column_size; j++) {
                char* temp = (char*) malloc(sizeof(char) * (strlen(identifiers->string[j]) + 3));
                sprintf(temp, "\"%s\"", identifiers->string[j]);
                temp[strlen(identifiers->string[j]) + 3] = '\0';
                if ((strcmp(current, identifiers->string[j]) == 0) || (strcmp(current, temp) == 0)) {
                    condition_id = j;
                    found = 1;
                    free(temp);
                    break;
                }
                free(temp);
            }
            if (found == 0) {
                sprintf(slog, "Invalid Query: '%s'\n", input);
                write(logFileFD, slog, strlen(slog) * sizeof(char));
                char** empty_str = (char**) malloc(sizeof(char*) * 1);
                char* temp = "empty database";
                empty_str[0] = temp;
                struct Database* empty = init_database(1, empty_str);
                free(empty_str);
                return empty;
            }
            current = current + strlen(current) + 1;
            condition_rule = (char*) malloc(sizeof(char) * (strlen(current) + 1));
            strcpy(condition_rule, current);
        } else {
            sprintf(slog, "Invalid Query: '%s'\n", input);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* temp = "empty database";
            empty_str[0] = temp;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
    } else if (strcmp(current, "TABLE;") != 0) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    struct Database* returnTable;

    pthread_mutex_lock(&lockThread);
    while ((active_writers + waiting_writers) > 0) {
        waiting_readers++;
        pthread_cond_wait(&condRead, &lockThread);
        waiting_readers--;
    }
    active_readers++;
    pthread_mutex_unlock(&lockThread);

    //ACCESS DATABASE
    struct row_node* temp = pDatabase->first_row;
    for (int i = 0; i < pDatabase->row_size; i++) {
        char** column = (char**) malloc(sizeof(char*) * column_size);
        for (int j = 0; j < column_size; j++) {
            column[j] = temp->string[column_id[j]];
        }
        if (i == 0) {
            returnTable = init_database(column_size, column);
        } else {
            if (condition_id == -1) {
                if (distinct_flag == 0)
                    add_row(returnTable, column);
                else {
                    int dflag = 0;
                    struct row_node* start = returnTable->first_row;
                    for (int k = 0; k < returnTable->row_size; k++) {
                        int f_flag = 0;
                        for (int l = 0; l < column_size; l++) {
                            if (strcmp(column[l], start->string[l]) == 0) {
                                f_flag = 1;
                            } else {
                                f_flag = 0;
                                break;
                            }
                        }
                        if (f_flag == 1) {
                            dflag = 1;
                            break;
                        }
                        start = start->next_row;
                    }
                    if (dflag == 0)
                        add_row(returnTable, column);
                }
            } else {
                char* string = (char*) malloc(sizeof(char) * (strlen(temp->string[condition_id]) + 3));
                sprintf(string, "\"%s\"", temp->string[condition_id]);
                string[strlen(condition_rule) + 3] = '\0';
                if ((strcmp(condition_rule, temp->string[condition_id]) == 0) || (strcmp(condition_rule, string) == 0)) {
                    if (distinct_flag == 0)
                        add_row(returnTable, column);
                    else {
                        int dflag = 0;
                        struct row_node* start = returnTable->first_row;
                        for (int k = 0; k < returnTable->row_size; k++) {
                            int f_flag = 0;
                            for (int l = 0; l < column_size; l++) {
                                if (strcmp(column[l], start->string[l]) == 0) {
                                    f_flag = 1;
                                } else {
                                    f_flag = 0;
                                    break;
                                }
                            }
                            if (f_flag == 1) {
                                dflag = 1;
                                break;
                            }
                            start = start->next_row;
                        }
                        if (dflag == 0)
                            add_row(returnTable, column);
                    }
                }
                free(string);
            }
        }
        temp = temp->next_row;
        free(column);
    }

    pthread_mutex_lock(&lockThread);
    active_readers--;
    if (active_readers == 0 && waiting_writers > 0)
        pthread_cond_signal(&condWrite);
    pthread_mutex_unlock(&lockThread);

    if (condition_id != -1)
        free(condition_rule);
    free(column_id);
    return returnTable;
}

struct Database *update_database(struct Database *pDatabase, char str[2048], struct pool_t *pPool) {
    //PARSE SQL
    int condition_id = -1;
    char* condition_rule = NULL;
    int* column_id;
    char** column_str;
    int column_size;
    char input[2048];
    char slog[2148];

    char* current;
    unsigned int size = strlen(str);
    int c = 0;
    int comma = 0;
    int flag = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] == ' ' && flag == 0) {
            str[i] = '\0';
            c++;
        }
        if (str[i] == ',' && flag == 0) {
            comma = comma + 1;
        }
        if (str[i] == ';') {
            c++;
        }
        if (str[i] == '\'' && flag == 0) {
            flag = 1;
            str[i] = '"';
        }
        else if (str[i] == '\'' && flag == 1) {
            flag = 0;
            str[i] = '"';
        }
    }
    strcpy(input, str);
    current = str;
    if ((strcmp(current, "UPDATE") != 0) || c < 4) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    current = current + strlen(current) + 1;
    if (strcmp(current, "TABLE") != 0) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    current = current + strlen(current) + 1;
    if (strcmp(current, "SET") != 0) {
        sprintf(slog, "Invalid Query: '%s'\n", input);
        write(logFileFD, slog, strlen(slog) * sizeof(char));
        char** empty_str = (char**) malloc(sizeof(char*) * 1);
        char* temp = "empty database";
        empty_str[0] = temp;
        struct Database* empty = init_database(1, empty_str);
        free(empty_str);
        return empty;
    }
    current = current + strlen(current) + 1;
    column_id = (int*) malloc(sizeof(int) * (comma + 1));
    column_str = (char**) malloc(sizeof(char*) * (comma + 1));
    column_size = comma + 1;
    int end_flag = 0;
    for (int i = 0; i < column_size; i++) {
        unsigned int _size = strlen(current);
        int _flag = 0;
        for (int j = 0; j < _size; j++) {
            if (current[j] == '=') {
                _flag = 1;
                current[j] = '\0';
                break;
            }
        }
        if (_flag == 0) {
            sprintf(slog, "Invalid Query: '%s'\n", input);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* temp = "empty database";
            empty_str[0] = temp;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
        int found = 0;
        struct row_node* temp = pDatabase->first_row;
        for (int j = 0; j < pDatabase->column_size; j++) {
            if (strcmp(temp->string[j], current) == 0) {
                column_id[i] = j;
                found = 1;
                break;
            }
        }
        if (found == 0) {
            sprintf(slog, "Error 1: could not prepare statement (1 no such column: %s)\n", current);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* string = "empty database";
            empty_str[0] = string;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
        current = current + strlen(current) + 1;
        int c_flag = 0;
        if (current[strlen(current) - 1] == ','){
            c_flag = 1;
            current[strlen(current) - 1] = '\0';
        }
        else if (current[strlen(current) - 1] == ';'){
            end_flag = 1;
            current[strlen(current) - 1] = '\0';
        }
        column_str[i] = (char*) malloc(sizeof(char) * (strlen(current) + 2));
        column_str[i][strlen(current) + 1] = '\0';
        strcpy(column_str[i], current);
        if (c_flag == 0)
            current = current + strlen(current) + 1;
        else
            current = current + strlen(current) + 2;
    }
    if (end_flag == 0) {
        if (strcmp(current, "WHERE") != 0) {
            sprintf(slog, "Invalid Query: '%s'\n", input);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* temp = "empty database";
            empty_str[0] = temp;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
        current = current + strlen(current) + 1;
        unsigned int _size = strlen(current);
        int _flag = 0;
        for (int j = 0; j < _size; j++) {
            if (current[j] == '=') {
                _flag = 1;
                current[j] = '\0';
                break;
            }
        }
        if (_flag == 0) {
            sprintf(slog, "Invalid Query: '%s'\n", input);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* temp = "empty database";
            empty_str[0] = temp;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }

        int found = 0;
        struct row_node* temp = pDatabase->first_row;
        for (int j = 0; j < pDatabase->column_size; j++) {
            if (strcmp(temp->string[j], current) == 0) {
                condition_id = j;
                found = 1;
                break;
            }
        }
        if (found == 0) {
            sprintf(slog, "Error 1: could not prepare statement (1 no such column: %s)\n", current);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* string = "empty database";
            empty_str[0] = string;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
        current = current + strlen(current) + 1;
        if (current[strlen(current) - 1] == ';'){
            end_flag = 1;
            current[strlen(current) - 1] = '\0';
        }
        if (end_flag == 0) {
            sprintf(slog, "Error 1: could not prepare statement (1 no such column: %s)\n", current);
            write(logFileFD, slog, strlen(slog) * sizeof(char));
            char** empty_str = (char**) malloc(sizeof(char*) * 1);
            char* string = "empty database";
            empty_str[0] = string;
            struct Database* empty = init_database(1, empty_str);
            free(empty_str);
            return empty;
        }
        condition_rule = (char*) malloc(sizeof(char) * (strlen(current) + 1));
        strcpy(condition_rule, current);
    }

    pthread_mutex_lock(&lockThread);
    while ((active_readers + active_writers) > 0) {
        waiting_writers++;
        pthread_cond_wait(&condWrite, &lockThread);
        waiting_writers--;
    }
    active_writers++;
    pthread_mutex_unlock(&lockThread);

    //ACCESS DATABASE
    struct row_node* temp = pDatabase->first_row->next_row;
    for (int i = 1; i < pDatabase->row_size; i++) {
        if (condition_id == -1) {
            char **replace = temp->string;
            char **new = (char**) malloc(sizeof(char*) * column_size);
            for (int j = 0; j < pDatabase->column_size; j++) {
                int found = -1;
                for (int k = 0; k < column_size; k++) {
                    if (column_id[k] == j) {
                        found = k;
                        break;
                    }
                }
                if (found == -1) {
                    new[j] = (char*) malloc(sizeof(char) * (strlen(replace[j]) + 1));
                    strcpy(new[j], replace[j]);
                } else {
                    new[j] = (char*) malloc(sizeof(char) * (strlen(column_str[found]) + 1));
                    strcpy(new[j], column_str[found]);
                }
            }
            temp->string = new;
            for (int j = 0; j < pDatabase->column_size; j++) {
                free(replace[j]);
            }
            free(replace);
        } else {
            char* string = (char*) malloc(sizeof(char) * (strlen(temp->string[condition_id]) + 3));
            sprintf(string, "\"%s\"", temp->string[condition_id]);
            string[strlen(temp->string[condition_id]) + 2] = '\0';
            if ((strcmp(temp->string[condition_id], condition_rule) == 0) || (strcmp(string, condition_rule) == 0)) {
                char **replace = temp->string;
                char **new = (char**) malloc(sizeof(char*) * pDatabase->column_size);
                for (int j = 0; j < pDatabase->column_size; j++) {
                    int found = -1;
                    for (int k = 0; k < column_size; k++) {
                        if (column_id[k] == j) {
                            found = k;
                            break;
                        }
                    }
                    if (found == -1) {
                        new[j] = (char*) malloc(sizeof(char) * (strlen(replace[j]) + 1));
                        strcpy(new[j], replace[j]);
                    } else {
                        new[j] = (char*) malloc(sizeof(char) * (strlen(column_str[found]) + 1));
                        strcpy(new[j], column_str[found]);
                    }
                }
                temp->string = new;
                for (int j = 0; j < pDatabase->column_size; j++) {
                    free(replace[j]);
                }
                free(replace);
            }
            free(string);
        }
        temp = temp->next_row;
    }

    struct Database* returnTable;
    struct row_node* firstNode = pDatabase->first_row;
    for (int i = 0; i < pDatabase->row_size; i++) {
        if (i == 0)
            returnTable = init_database(pDatabase->column_size, firstNode->string);
        else
            add_row(returnTable, firstNode->string);
        firstNode = firstNode->next_row;
    }


    pthread_mutex_lock(&lockThread);
    active_writers--;
    if (waiting_writers > 0)
        pthread_cond_signal(&condWrite);
    else if (waiting_readers > 0)
        pthread_cond_broadcast(&condRead);
    pthread_mutex_unlock(&lockThread);
    for (int i = 0; i < column_size; i++) {
        free(column_str[i]);
    }
    free(column_str);
    free(condition_rule);
    free(column_id);
    return returnTable;
}


int becomeDaemon(int flags) {           /*  Returns 0 on success, -1 on error */
    int maxfd, fd;

    switch (fork()) {                   /*  Become background process   */
        case -1: return -1;
        case 0: break;                  /*  Child falls through; adopted by init */
        default: _exit(EXIT_SUCCESS);   /*  parent terminates and shell prompt is back */
    }

    if (setsid() == -1)                 /*  Become leader of new session, dissociate from tty */
        return -1;                      /*  can still acquire a controlling terminal    */

    switch (fork()) {                   /*  Ensure we are not session leader    */
        case -1: return -1;             /*  thanks to 2nd fork , there is no way to acquiring a tty */
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if (!(flags & BD_NO_UMASK0))
        umask(0);                       /* Clear file mode creation mask    */
    if (!(flags & BD_NO_CHDIR))
        chdir("/");                     /* Change to root directory */
    if (!(flags & BD_NO_CLOSE_FILES)) {      /* Close all open files */
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)                     /* Limit is inderminate... */
            maxfd = BD_MAX_CLOSE;            /* so take a guess */
        for (fd = 0; fd < maxfd; fd++)
            close(fd);
    }
    if (!(flags & BD_NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);                 /* Reopen standard fd's to /dev/null */
        fd = open("/dev/null", O_RDWR);
        if (fd != STDIN_FILENO)              /* 'fd' should be 0 */
            return -1;
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -1;
    }
    return 0;
}
