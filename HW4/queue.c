#include <stdlib.h>
#include "queue.h"

struct Queue* createQueue(unsigned capacity) {
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->capacity =capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (char*) malloc(queue->capacity * sizeof(char));
    return queue;
}

int isFull(struct Queue* queue) {
    return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue) {
    return queue->size == 0;
}

void enqueue(struct Queue* queue, char item) {
    if (isFull(queue))
        return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

char dequeue(struct Queue* queue) {
    if (isEmpty(queue))
        return '\0';
    char item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

char front(struct Queue* queue) {
    if (isEmpty(queue))
        return '\0';
    return queue->array[queue->front];
}

char rear(struct Queue* queue) {
    if (isEmpty(queue))
        return '\0';
    return queue->array[queue->rear];
}
