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
#define CLOCK_MONOTONIC 1 //for some reason my ide can't see it in time.h

/*
NOTES
DO NOT FORGET TO DETACH MEMORY AND SEMAPHORES!!!
possibly all operations on semaphores should use undo flag in case of termination

*/


struct Queue* qctl;
int *qarr;
sem_t *sems[3] = {NULL, NULL, NULL};

typedef unsigned short int ushort;
union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

void intsig(int signum){
    //detach shmem
    if(shmdt(qarr)<0){perror("shmdt qarr");}
    if(shmdt(qctl)<0){perror("shmdt qctl");}

    printf("\nreceived SIGINT\n");
    exit(EXIT_SUCCESS);
}

void finish(){
    if(shmdt(qarr)<0){perror("shmdt qarr");}
    if(shmdt(qctl)<0){perror("shmdt qctl");}

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
    int qctlkey = shmget(KEY_QCNTL, sizeof(struct Queue), 0644); if(qctlkey<0){perror("shmget qctl"); exit(EXIT_FAILURE);};
    qctl = (struct Queue*)shmat(qctlkey, (void *)0, 0); if(qctl==(struct Queue*)(-1)){perror("shmat qctl"); exit(EXIT_FAILURE);}


    int qarrkey = shmget(KEY_QUEUE, qctl->capacity * sizeof(int), 0644); if(qarrkey<0){perror("shmget qarr"); exit(EXIT_FAILURE);};
    qarr = (int*)shmat(qarrkey, (void *)0, 0); if(qarr==(int*)(-1)){perror("shmat qarr"); exit(EXIT_FAILURE);}

    int semid = semget(KEY_SEM, 0, 0666); if(semid < 0){perror("semget"); exit(EXIT_FAILURE);}
    //semphores:
    //0:waiting room (can be accessed?) def 1
    //1:client (ready?) (or signal) def 0
    //2:barber (ready?) (or signal) def 0

    int children = dec(argv[1]);
    int haircuts = dec(argv[2]);

    for(int i=0; i<children; i++){
        int child = fork();
        if(!child){
            for(int j=0; j<haircuts; j++){
                struct sembuf params[1];
                struct timespec ts;
                //decrease waiting room semaphore - customer entered, need to lock queue to get him from there (wait if blocked)
                if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                printf("%li,%li: %d entering waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = -1;
                if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

                //check queue->capacity - queue->size to get free seats, if > 0
                if(qctl->capacity > qctl->size){
                    //enqueue yourself (free seats number automatically decreases)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d sitting in waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    enqueue(qctl, qarr, getpid());

                    //increase client semaphore - ready for cut (client's currently in waiting room)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d ready for cut\n", ts.tv_sec, ts.tv_nsec, getpid());
                    params[0].sem_flg = 0; params[0].sem_num = 1; params[0].sem_op = 1;
                    if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

                    //increase waiting room semaphore - lock no longer needed
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d unlocking waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = 1;
                    if (semop(semid, params, 1) == -1){perror("semop");} //no exit here? 

                    //decrease barber semaphore - wait for barber to get client
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d waiting for barber\n", ts.tv_sec, ts.tv_nsec, getpid());
                    params[0].sem_flg = 0; params[0].sem_num = 2; params[0].sem_op = -1;
                    if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?
                    //(Have hair cut here.)
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d having haircut\n", ts.tv_sec, ts.tv_nsec, getpid());

                }else{ //no free seats, client leaves
                if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d no free seats\n", ts.tv_sec, ts.tv_nsec, getpid());
                    
                    // increase waiting room semaphore - client leaves
                    if(clock_gettime(1, &ts) == -1){perror("clk_get");}
                    printf("%li,%li: %d leaving waiting room\n", ts.tv_sec, ts.tv_nsec, getpid());
                    params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = 1;
                    if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

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