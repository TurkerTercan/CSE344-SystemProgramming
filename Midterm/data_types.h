#ifndef MIDTERM_DATA_TYPES_H
#define MIDTERM_DATA_TYPES_H

#define FILELENGTH 100
#define INTEGERBITSIZE 64

struct reg_citizen {
    pid_t pid;
    int count;
    int busy_flag;
};

const char a = 'a';
const char *clinic_fifo = "/tmp/GTU344Clinic";

#endif //MIDTERM_DATA_TYPES_H
