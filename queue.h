#pragma once
#define QUEUE_H

struct Queue
{
    int front, rear, size;
    unsigned capacity;
};


int isFull(struct Queue* queue);

int isEmpty(struct Queue* queue);

void enqueue(struct Queue* queue, int* array, int item);

int dequeue(struct Queue* queue, int* array);

int front(struct Queue* queue, int* array);