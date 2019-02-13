#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/sem.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include "cstm.h"
#include <unistd.h>

#include "queue.h"


/*
struct Queue* createQueue(unsigned capacity)
{
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;  // This is important, see the enqueue
    //queue->array = (int*) malloc(queue->capacity * sizeof(int));
    queue->size = sizeof(struct Queue) + queue->capacity*sizeof(int);
    return queue;
}

void destroyQueue(struct Queue* queue){
    //free(queue->array);
    //free(queue);
}*/
 
// Queue is full when size becomes equal to the capacity 
int isFull(struct Queue* queue)
{  return (queue->size == queue->capacity);  }
 
// Queue is empty when size is 0
int isEmpty(struct Queue* queue)
{  return (queue->size == 0); }
 
// Function to add an item to the queue.  
// It changes rear and size
void enqueue(struct Queue* queue, int* array, int item)
{
    if(isFull(queue)){
        return;
    }
    queue->rear = (queue->rear + 1)%queue->capacity;
    array[queue->rear] = item;
    queue->size = queue->size + 1;
    printf("%d enqueued to queue\n", item);
}
 
// Function to remove an item from queue. 
// It changes front and size
int dequeue(struct Queue* queue, int* array)
{
    if(isEmpty(queue)){
        return INT_MIN;
    }
    int item = array[queue->front];
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
    return item;
}
 
int front(struct Queue* queue, int* array)
{
    if(isEmpty(queue)){
        return INT_MIN;
    }
    return array[queue->front];
}
 
int rear(struct Queue* queue, int* array)
{
    if(isEmpty(queue)){
        return INT_MIN;
    }
    return array[queue->rear];
}