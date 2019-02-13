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
#include "queue.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#define CLOCK_MONOTONIC 1 //for some reason my ide can't see it in time.h

/*
NOTES
all of the exit_failure should call function to cleanup attached memory and ipcs
add info printing, like barber status, semaphores locked clients pids and stuff

*/

int MAX_CLIENTS;
sem_t *sems[3] = {NULL, NULL, NULL};
int qcntl;
int qarr;

typedef unsigned short int ushort;
union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
};


void intsig(int signum){
    //cleanup shared mem
    if(sem_close(sems[0]) == -1){perror("sem_close");}
    if(sem_close(sems[1]) == -1){perror("sem_close");}
    if(sem_close(sems[2]) == -1){perror("sem_close");}

    if(munmap(qarr, MAX_CLIENTS * sizeof(int)) == -1){perror("munmap");}
    if(shm_unlink("qarr") == -1){perror("shm_unlink");}

    if(munmap(qcntl, sizeof(struct Queue)) == -1){perror("munmap");}
    if(shm_unlink("qcntl") == -1){perror("shm_unlink");}
    printf("\nreceived SIGINT\n");
    exit(EXIT_SUCCESS);
}

long dec(char *num){
    return strtol(num, NULL, 10);
}


int main(int argc, char* argv[]){
    signal(SIGINT, intsig);
    
    if(argc != 2){ printf("missing #chairs\n"); exit(EXIT_FAILURE);}

    //will define queue capacity
    int MAX_CLIENTS = dec(argv[1]); //globally?

    //create a queue (mind that clients shouldnt use ipc_creat in case of fail)
    int qcntld = shm_open("qcntl", O_RDWR | IPC_CREAT | IPC_EXCL, 0666);if(qcntld == -1){perror("shm_open");exit(EXIT_FAILURE);}
    if(ftruncate(qcntld, sizeof(struct Queue)) == -1){perror("ftruncate");exit(EXIT_FAILURE);}
    struct Queue *qcntl = mmap(NULL, sizeof(struct Queue), PROT_READ | PROT_WRITE, MAP_SHARED, qcntld, 0);

    int qarrd = shm_open("qarr", O_RDWR | IPC_CREAT | IPC_EXCL, 0666);if(qcntl == -1){perror("shm_open");exit(EXIT_FAILURE);}
    if(ftruncate(qarrd, MAX_CLIENTS * sizeof(int)) == -1){perror("ftruncate");exit(EXIT_FAILURE);}
    int *qarr = mmap(NULL, MAX_CLIENTS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, qarrd, 0);

    

    //setup queue
    qcntl->capacity = MAX_CLIENTS;
    qcntl->front = qcntl->size = 0;
    qcntl->rear = MAX_CLIENTS - 1;

    //setup semaphore for:
    //0:waiting room (can be accessed?) def 1
    //1:client (ready?) (or signal) def 0
    //2:barber (ready?) (or signal) def 0
    sems[0] = sem_open("wroom", IPC_EXCL | IPC_CREAT, 0666, 1);if(sems[0] == NULL){perror("sem_open");}
    sems[1] = sem_open("client", IPC_EXCL | IPC_CREAT, 0666, 0);if(sems[1] == NULL){perror("sem_open");}
    sems[2] = sem_open("barber", IPC_EXCL | IPC_CREAT, 0666, 0);if(sems[2] == NULL){perror("sem_open");}
    //now we should have all semaphores created and set up

    //if(sem_post(sems[0]) == -1){perror("sem_post");}
    //if(sem_wait(sems[0]) == -1){perror("sem_wait");}
    
    while(1){
        struct sembuf params[1];
        struct timespec ts;

        //decrease client semphore - sleep while no customers
        if(clock_gettime(1, &ts) == -1){perror("clk_get");}
        printf("%li,%li: sleeping\n", ts.tv_sec, ts.tv_nsec);
        if(sem_wait(sems[1]) == -1){perror("sem_wait");}

        //decrease waiting room semaphore - customer entered, need to lock queue to get him from there (wait if blocked)
        int clientid = dequeue(qcntl, qarr);
        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: awoken by %d\n", ts.tv_sec, ts.tv_nsec, clientid);
        if(sem_wait(sems[0]) == -1){perror("sem_wait");}

        //increase number of free seats (queue size--) and dequeue client
        //automatic when dequeuing but is it needed for anything but printing customer info?
        //i assume that this is its only purpose for now

        //add some way for clients to let them know which one is currently taken care of?
        //some clever adding/subtracting from queue id and size? or simply some kind of incrementing ids
        //or if i have (and i will) the clients pid i can just signal them with forex sig_usr
        //will see after implementing clients first draft

        //increase barber semaphore - ready to work
        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: %d in chair\n", ts.tv_sec, ts.tv_nsec, clientid);
        if(sem_post(sems[2]) == -1){perror("sem_post");}

        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: cutting %d\n", ts.tv_sec, ts.tv_nsec, clientid);

        //increase waiting room semaphore - lock no longer needed
        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: cutting done\n", ts.tv_sec, ts.tv_nsec);
        if(sem_post(sems[2]) == -1){perror("sem_post");}
    }


   
    //while(1){
        //block the queue (or smthing else so the joining client will have to wait for barber to sleep), 
        //                  maybe something like can enter and if blocked, the client wont be able to send msg
        //                  also, when there'll be clients waiting, the semaphore could be higher allowing them to join, 
        //                  'cause new clients wont cause problems with sync
        //                  (so basically decrement once for joining, after succes, increment twice, after leaving decrement twice)
        //check the queue
        //if queue empty
        //    unblock
        //    go to sleep
        //else
        //    get the client
        //    cut the client
        //    

    //}
}