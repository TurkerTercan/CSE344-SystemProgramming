//
// Created by ttwicer on 4.06.2021.
//

#ifndef FINAL_DATABASE_H
#define FINAL_DATABASE_H

#include <stdlib.h>
#include <string.h>
#include "server.h"

struct row_node {
    int column_size;
    char** string;
    struct row_node* next_row;
};

struct Database {
    int column_size;
    int row_size;
    struct row_node* first_row;
    struct row_node* tail;
};

struct Database* init_database(int columnSize, char **column);
void close_database(struct Database* database);
struct Database *parse_csv(int csvFD, int logFD);
void add_row(struct Database *database, char** column);

#endif //FINAL_DATABASE_H
