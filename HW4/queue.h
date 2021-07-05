#ifndef HW4_QUEUE_H
#define HW4_QUEUE_H

struct Queue {
    int front, rear, size;
    unsigned capacity;
    char* array;
};

struct Queue* createQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, char item);
char dequeue(struct Queue* queue);
char front(struct Queue* queue);
char rear(struct Queue* queue);


#endif //HW4_QUEUE_H
