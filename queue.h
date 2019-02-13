//source: https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
#pragma once
#define QUEUE_H
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

// A structure to represent a queue
struct _Queue{
    int front, rear, size;
    unsigned capacity;
    int* array;
};
 
typedef struct _Queue Queue;
// function to create a queue of given capacity. 
// It initializes size of queue as 0
Queue* createQueue(unsigned int capacity){
    Queue* queue = (Queue*) malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;  // This is important, see the enqueue
    queue->array = (int*) malloc(queue->capacity * sizeof(int));
    return queue;
};

void destroyQueue(Queue* queue){
    free(queue->array);
}
 
// Queue is full when size becomes equal to the capacity 
int isFull(Queue* queue)
{  return (queue->size == queue->capacity);  }
 
// Queue is empty when size is 0
int isEmpty(Queue* queue)
{  return (queue->size == 0); }
 
// Function to add an item to the queue.  
// It changes rear and size
void enqueue(Queue* queue, int item){
    if (isFull(queue)){
        return;
    }
    queue->rear = (queue->rear + 1)%queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}
 
// Function to remove an item from queue. 
// It changes front and size
int dequeue(Queue* queue){
    if (isEmpty(queue)){
        return INT_MIN;
    }
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
    return item;
}
 
// Function to get front of queue
int front(Queue* queue){
    if (isEmpty(queue)){
        return INT_MIN;
    }
    return queue->array[queue->front];
}
 
// Function to get rear of queue
int rear(Queue* queue){
    if (isEmpty(queue)){
        return INT_MIN;
    }
    return queue->array[queue->rear];
}