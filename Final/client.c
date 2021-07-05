//
// Created by ttwicer on 4.06.2021.
//
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define true 1
#define false 0

int terminated = 0;

void interrupt_handler(int signum) {
    if (signum == SIGINT)
        terminated = true;
}

struct dynamic_query {
    char* query;
    int size;
    struct dynamic_query* next;
    struct dynamic_query* last;
};

void add_query(struct dynamic_query* first, char* input) {
    if (first->size == 0) {
        first->query = (char*) malloc(sizeof(char) * (strlen(input) + 1));
        strcpy(first->query, input);
        first->next = NULL;
        first->last = first;
        first->size = 1;
    } else {
        struct dynamic_query *temp = (struct dynamic_query*) malloc(sizeof(struct dynamic_query));
        temp->last = NULL;
        temp->next = NULL;
        temp->query = (char*) malloc(sizeof(char) * (strlen(input) + 1));
        strcpy(temp->query, input);
        first->last->next = temp;
        first->last = first->last->next;
        first->size = first->size + 1;
    }
}

void free_query(struct dynamic_query* first) {
    for (int i = first->size - 1; i >= 0; i--) {
        struct dynamic_query* temp = first;
        for (int j = 0; j < i; j++) {
            temp = temp->next;
        }
        free(temp->query);
        free(temp);
    }
}

void format_time(char *output){
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    sprintf(output, "%d %d %d %d:%d:%d",timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

int main(int argc, char* argv[]) {
    int id;
    int port;
    char* usage = "./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile";
    char* pathToQueryFile;
    char* IPAddr;
    int opt, a_flag = 0, p_flag = 0, o_flag = 0, i_flag = 0;
    struct dynamic_query* dynamicQuery = (struct dynamic_query*) malloc(sizeof(struct dynamic_query));
    dynamicQuery->size = 0;

    while ((opt = getopt(argc, argv, "a:p:o:i:")) != -1) {
        switch (opt) {
            case 'a':
                IPAddr = (char *) malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(IPAddr, optarg);
                a_flag = 1;
                break;
            case 'p':
                port = atoi(optarg);
                p_flag = 1;
                break;
            case 'o':
                pathToQueryFile = (char *) malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(pathToQueryFile, optarg);
                o_flag = 1;
                break;
            case 'i':
                id = atoi(optarg);
                i_flag = 1;
                break;
            default:
                break;
        }
    }

    if (a_flag == 0 || o_flag == 0 || p_flag == 0 || i_flag == 0 || port < 1000 || id < 0) {
        if (port < 1000)
            fprintf(stdout, "Invalid Arguments!\nPORT > 1000\nUsage: %s\n", usage);
        else if (id < 0)
            fprintf(stdout, "Invalid Arguments!\nID >= 1\nUsage: %s\n", usage);
        else
            fprintf(stdout, "Invalid Arguments!\nUsage: %s\n", usage);
        if (o_flag == 1)
            free(pathToQueryFile);
        if (a_flag == 1)
            free(IPAddr);
        exit(EXIT_FAILURE);
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &interrupt_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        char* log = "SIGACTION SIGINT";
        write(STDOUT_FILENO, log, strlen(log));
        exit(EXIT_FAILURE);
    }


    int query_fd = open(pathToQueryFile, O_RDONLY, 0777);
    char byte = ' ';
    int size;
    int start = 0, end = 0;
    while (byte != '\0') {
        int comma = 0;
        while (byte != '\n') {
            size = read(query_fd, &byte, 1);
            if (size == -1 || size == 0)
                break;
            if (byte == '\0')
                break;
            if (byte == ',')
                comma = comma + 1;
            end++;
        }
        if (size == -1)
            break;
        lseek(query_fd, start, SEEK_SET);
        char *line = (char *) malloc(sizeof(char) * ((end - start) + comma + 1));
        while (((size = read(query_fd, line, sizeof(char) * (end - start))) == -1) &&
               (errno == EINTR) && terminated == 0);
        if (size == -1)
            break;
        int id_flag = 0;
        int x = 0;
        for (int i = 0; i < ((end - start) + x - 1); i++) {
            if (line[i] == ',' && line[i + 1] != ' ') {
                x++;
                for (int j = ((end - start) + x); j > i; j--) {
                    line[j] = line[j - 1];
                }
                line[i + 1] = ' ';
            }
        }

        for (int i = 0; i < (end - start) + x; i++) {
            if (id_flag == 0 && line[i] == ' ') {
                line[i] = '\0';
                id_flag = 1;
            }
            if (line[i] == '\n')
                line[i] = '\0';
        }
        line[(end-start+x)] = '\0';
        char* current = line;
        int query_id = atoi(current);
        if (query_id == id) {
            current = current + strlen(current) + 1;
            add_query(dynamicQuery, current);
        }
        free(line);
        start = end;
        byte = ' ';
        lseek(query_fd, end, SEEK_SET);
        if (size == 0)
            break;
    }

    char timestamp[1024];
    format_time(timestamp);
    fprintf(stdout, "(%s)Client-%d connecting to %s:%d\n", timestamp, id, IPAddr, port);

    //Client Connection
    int client_socket = 0;
    char receiver_buffer[1025] = " ";
    struct sockaddr_in server_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == 0) {
        perror("Error: Socket Creation");
        close(client_socket);
        free_query(dynamicQuery);
        free(IPAddr);
        free(pathToQueryFile);
        exit(EXIT_SUCCESS);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(IPAddr);

    int retVal = connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address));
    if (retVal < 0) {
        perror("Error: Connect");
        close(client_socket);
        free_query(dynamicQuery);
        free(IPAddr);
        free(pathToQueryFile);
        exit(EXIT_SUCCESS);
    }
    struct dynamic_query *temp = dynamicQuery;
    write(client_socket, &dynamicQuery->size, sizeof(int));
    for (int i = 0; i < dynamicQuery->size && terminated == 0; i++) {
        struct timeval tv1, tv2;
        gettimeofday(&tv1, NULL);
        format_time(timestamp);
        fprintf(stdout, "(%s)Client-%d connected an sending query '%s'\n", timestamp, id, temp->query);
        write(client_socket, temp->query, sizeof(char) * strlen(temp->query));
        int record;
        read(client_socket, &record, sizeof(int));
        gettimeofday(&tv2, NULL);
        format_time(timestamp);
        fprintf(stdout, "(%s)Server response to Client-%d is %d records, and arrived in %.2f seconds.\n", timestamp, id, record - 1,
                (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec));
        size = 1;
        receiver_buffer[size - 1] = ' ';
        byte = ' ';
        while (receiver_buffer[size - 1] != 4) { //receiver_buffer[size - 1] != 4
            while (((size = read(client_socket, receiver_buffer, sizeof(char) * 1024)) == -1) &&
                    (errno == EINTR) && terminated == 0);
            if (size <= 0)
                break;
            receiver_buffer[size] = '\0';
            receiver_buffer[1024] = '\0';
            for (int x = 0; x < strlen(receiver_buffer) && record != 0; x++) {
                if (receiver_buffer[x] != 4)
                    fprintf(stdout, "%c", receiver_buffer[x]);
            }
        }
        fflush(stdout);
        temp = temp->next;
    }
    format_time(timestamp);
    if (terminated == 0)
        fprintf(stdout, "\n(%s)A total of %d queries were executed, client is terminating.\n", timestamp, dynamicQuery->size);
    else
        fprintf(stdout, "\n(%s)SIGINT signal is received, client is terminating.\n", timestamp);


    close(client_socket);
    free_query(dynamicQuery);
    free(IPAddr);
    free(pathToQueryFile);
    exit(EXIT_SUCCESS);
}
