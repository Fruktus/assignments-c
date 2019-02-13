#include <stdlib.h>
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
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#define CLOCK_MONOTONIC 1 //for some reason my ide can't see it in time.h

/*
NOTES
DO NOT FORGET TO DETACH MEMORY AND SEMAPHORES!!!
possibly all operations on semaphores should use undo flag in case of termination

*/


struct Queue* qcntl;
int *qarr;
sem_t *sems[3] = {NULL, NULL, NULL};
int MAX_CLIENTS;

typedef unsigned short int ushort;
union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

void intsig(int signum){
    //detach shmem
    if(sem_close(sems[0]) == -1){perror("sem_close");}
    if(sem_close(sems[1]) == -1){perror("sem_close");}
    if(sem_close(sems[2]) == -1){perror("sem_close");}

    if(munmap(qarr, MAX_CLIENTS * sizeof(int)) == -1){perror("munmap");}
    if(munmap(qcntl, sizeof(struct Queue)) == -1){perror("munmap");}

    printf("\nreceived SIGINT\n");
    exit(EXIT_SUCCESS);
}

void finish(){
    if(sem_close(sems[0]) == -1){perror("sem_close");}
    if(sem_close(sems[1]) == -1){perror("sem_close");}
    if(sem_close(sems[2]) == -1){perror("sem_close");}

    if(munmap(qarr, MAX_CLIENTS * sizeof(int)) == -1){perror("munmap");}
    if(munmap(qcntl, sizeof(struct Queue)) == -1){perror("munmap");}

    exit(EXIT_SUCCESS);
}


long dec(char *num){
    return strtol(num, NULL, 10);
}


int main(int argc, char* argv[]){
    if(argc!=3){
        printf("missing params, #kids, #cuts\n");
        exit(EXIT_FAILURE);
    }
    
    //the queue should be now set up by barber, attach to it
    //first the controller to obtain max_clients for proper array attaching, should be in queue controller
    int qcntld = shm_open("qcntl", O_RDWR, 0666);if(qcntld == -1){perror("shm_open");exit(EXIT_FAILURE);}
    if(ftruncate(qcntld, sizeof(struct Queue)) == -1){perror("ftruncate");exit(EXIT_FAILURE);}
    struct Queue *qcntl = mmap(NULL, sizeof(struct Queue), PROT_READ | PROT_WRITE, MAP_SHARED, qcntld, 0);

    int qarrd = shm_open("qarr", O_RDWR, 0666);if(qcntl == -1){perror("shm_open");exit(EXIT_FAILURE);}
    if(ftruncate(qarrd, qcntl->capacity * sizeof(int)) == -1){perror("ftruncate");exit(EXIT_FAILURE);}
    int *qarr = mmap(NULL, qcntl->capacity * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, qarrd, 0);
    MAX_CLIENTS = qcntl->capacity;
    //semphores:
    //0:waiting room (can be accessed?) def 1
    //1:client (ready?) (or signal) def 0
    //2:barber (ready?) (or signal) def 0

    sems[0] = sem_open("wroom", 0, 0666, 1);if(sems[0] == NULL){perror("sem_open");}
    sems[1] = sem_open("client", 0, 0666, 0);if(sems[1] == NULL){perror("sem_open");}
    sems[2] = sem_open("barber", 0, 0666, 0);if(sems[2] == NULL){perror("sem_open");}

    int children = dec(argv[1]);
    int haircuts = dec(argv[2]);

    //if(sem_post(sems[0]) == -1){perror("sem_post");}
    //if(sem_wait(sems[0]) == -1){perror("sem_wait");}

    for(int i=0; i<children; i++){
        int child = fork();
        if(!child){
            for(int j=0; j<haircuts; j++){
                struct sembuf params[1];
                struct timespec ts;
                //decrease waiting room semaphore - customer entered, need to lock queue to get him from there (wait if blocked)
                if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                printf("%li,%li: %d entering waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                if(sem_wait(sems[0]) == -1){perror("sem_wait");}

                //check queue->capacity - queue->size to get free seats, if > 0
                if(qcntl->capacity > qcntl->size){
                    //enqueue yourself (free seats number automatically decreases)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d sitting in waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    enqueue(qcntl, qarr, getpid());

                    //increase client semaphore - ready for cut (client's currently in waiting room)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d ready for cut\n", ts.tv_sec, ts.tv_nsec, getpid());
                    if(sem_post(sems[1]) == -1){perror("sem_post");}

                    //increase waiting room semaphore - lock no longer needed
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d unlocking waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    if(sem_post(sems[0]) == -1){perror("sem_post");}

                    //decrease barber semaphore - wait for barber to get client
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d waiting for barber\n", ts.tv_sec, ts.tv_nsec, getpid());
                    if(sem_wait(sems[2]) == -1){perror("sem_wait");}

                    //(Have hair cut here.)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d having haircut\n", ts.tv_sec, ts.tv_nsec, getpid());

                }else{ //no free seats, client leaves
                if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d no free seats\n", ts.tv_sec, ts.tv_nsec, getpid());
                    
                    // increase waiting room semaphore - client leaves
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d leaving waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    if(sem_post(sems[0]) == -1){perror("sem_post");}

                    //(Leave without a haircut.)
                }
            }
            finish();
        }
    }
    //process stops without returning to prompt because parent does not wait for children to finish
    //but it actually works as it should
    exit(EXIT_SUCCESS);
}