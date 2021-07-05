#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "Database.h"

struct Database* init_database(int columnSize, char **column) {
    struct Database* database;
    database = (struct Database*) malloc(sizeof(struct Database));
    database->column_size = columnSize;
    struct row_node* temp = (struct row_node*) malloc(sizeof(struct row_node));
    temp->column_size = columnSize;
    temp->string = (char**) malloc(sizeof(char*) * columnSize);
    temp->next_row = NULL;

    for (int i = 0; i < columnSize; i++) {
        temp->string[i] = (char*) malloc(sizeof(char) * (strlen(column[i]) + 1));
        strcpy(temp->string[i], column[i]);
    }

    database->first_row = temp;
    database->tail = temp;
    database->row_size = 1;
    return database;
}

void add_row(struct Database *database, char** column) {
    struct row_node *temp = database->tail;

    struct row_node *new = (struct row_node*) malloc(sizeof(struct row_node));
    new->column_size = database->column_size;
    new->string = ((char**) malloc(sizeof(char*) * new->column_size));
    new->next_row = NULL;

    for (int i = 0; i < new->column_size; i++) {
        new->string[i] = (char*) malloc(sizeof(char) * (strlen(column[i]) + 1));
        strcpy(new->string[i], column[i]);
    }

    temp->next_row = new;
    database->tail = database->tail->next_row;
    database->row_size = database->row_size + 1;
}

struct Database *parse_csv(int csvFD, int logFD) {
    clock_t first = clock();
    char log[1048] = "Loading dataset...\n";
    write(logFD, log, strlen(log));
    struct Database *database;

    int start = 0, end = 0, size, row = 0;
    char byte = ' ';

    while (byte != '\0') {
        while (byte != '\n') {
            size = read(csvFD, &byte, 1);
            if (size == -1 || size == 0)
                break;
            if (byte == EOF)
                break;
            end++;
        }
        if (byte == '\0' || size == 0 || size == -1)
            break;
        lseek(csvFD, start, SEEK_SET);
        char *line = (char *) malloc(sizeof(char) * (end - start));
        size = read(csvFD, line, sizeof(char) * (end - start));
        if (size == -1)
            break;
        int count = 0;
        int parenthesis = 0;
        for (int i = 0; i < (end - start); i++) {
            if ((line[i] == '\n' || line[i] == ',') && parenthesis == 0) {
                line[i] = '\0';
                count++;
            }
            if (line[i] == '"' && parenthesis == 0)
                parenthesis = 1;
            else if(line[i] == '"' && parenthesis == 1)
                parenthesis = 0;
        }
        char **columns = (char **) malloc(sizeof(char *) * count);
        int j = 0;
        for (int i = 0; i < (end - start - 1) && j < count; i++) {
            if (i == 0)
                columns[j++] = line;
            else if (line[i] == '\0')
                columns[j++] = line + i + 1;
        }

        if (row == 0) {
            database = init_database(count, columns);
        }
        else {
            add_row(database, columns);
        }

        free(columns);
        free(line);
        row++;
        byte = ' ';
        start = end;
        lseek(csvFD, end, SEEK_SET);
    }
    clock_t last = clock();
    double time = (double)(last - first) / CLOCKS_PER_SEC;
    sprintf(log, "Dataset loaded in %.2f seconds with %d records.\n", time, (database->row_size - 1));
    write(logFD, log, strlen(log));
    return database;
}

void close_database(struct Database* database) {
    struct row_node* temp = database->first_row;
    for (int i = 0; i < database->row_size; i++) {
        struct row_node* next = temp->next_row;
        for (int j = 0; j < database->column_size; j++) {
            free(temp->string[j]);
        }
        free(temp->string);
        free(temp);
        if (i != database->row_size - 1)
            temp = next;
    }
    free(database);
}