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
#define CLOCK_MONOTONIC 1 //for some reason my ide can't see it in time.h

/*
NOTES
all of the exit_failure should call function to cleanup attached memory and ipcs
add info printing, like barber status, semaphores locked clients pids and stuff

*/

int qarrkey;
int qctlkey;
int semid;
struct Queue* qctl;
int *qarr;


typedef unsigned short int ushort;
union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
};


void intsig(int signum){
    //cleanup shared mem
    if(shmdt(qarr)<0){perror("shmdt qarr");}
    if(shmdt(qctl)<0){perror("shmdt qctl");}

    shmctl(qarrkey, IPC_RMID, 0);
    shmctl(qctlkey, IPC_RMID, 0);
    semctl(semid, 3, IPC_RMID);
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
    qctlkey = shmget(KEY_QCNTL, sizeof(struct Queue), 0644 | IPC_CREAT | IPC_EXCL); if(qctlkey<0){perror("shmget qctl"); exit(EXIT_FAILURE);};
    qarrkey = shmget(KEY_QUEUE, MAX_CLIENTS * sizeof(int), 0644 | IPC_CREAT | IPC_EXCL); if(qarrkey<0){perror("shmget qarr"); exit(EXIT_FAILURE);};

    qctl = (struct Queue*)shmat(qctlkey, (void *)0, 0); if(qctl==(struct Queue*)(-1)){perror("shmat qctl"); exit(EXIT_FAILURE);}
    qarr = (int*)shmat(qarrkey, (void *)0, 0); if(qarr==(int*)(-1)){perror("shmat qarr"); exit(EXIT_FAILURE);}

    //setup queue
    qctl->capacity = MAX_CLIENTS;
    qctl->front = qctl->size = 0;
    qctl->rear = MAX_CLIENTS - 1;

    //setup semaphore for:
    //0:waiting room (can be accessed?) def 1
    //1:client (ready?) (or signal) def 0
    //2:barber (ready?) (or signal) def 0

    semid = semget(KEY_SEM, 3, 0666 | IPC_CREAT | IPC_EXCL); //przy dolaczaniu nsems = 0
    if(semid < 0){perror("semget"); exit(EXIT_FAILURE);}


    union semun arg;
    
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1){perror("semctl");exit(EXIT_FAILURE);}
    arg.val = 0;
    if (semctl(semid, 1, SETVAL, arg) == -1){perror("semctl");exit(EXIT_FAILURE);} 
    if (semctl(semid, 2, SETVAL, arg) == -1){perror("semctl");exit(EXIT_FAILURE);} 
    //now we should have all semaphores created and set up

    while(1){
        struct sembuf params[1];
        struct timespec ts;

        //decrease client semphore - sleep while no customers
        if(clock_gettime(1, &ts) == -1){perror("clk_get");}
        printf("%li,%li: sleeping\n", ts.tv_sec, ts.tv_nsec);
        params[0].sem_flg = 0; params[0].sem_num = 1; params[0].sem_op = -1;
        if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

        //decrease waiting room semaphore - customer entered, need to lock queue to get him from there (wait if blocked)
        int clientid = dequeue(qctl, qarr);
        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: awoken by %d\n", ts.tv_sec, ts.tv_nsec, clientid);
        params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = -1;
        if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

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
        params[0].sem_flg = 0; params[0].sem_num = 2; params[0].sem_op = 1;
        if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: cutting %d\n", ts.tv_sec, ts.tv_nsec, clientid);

        //increase waiting room semaphore - lock no longer needed
        if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){perror("clk_get");}
        printf("%li,%li: cutting done\n", ts.tv_sec, ts.tv_nsec);
        params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = 1;
        if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?
    }


    //pointless here since loop 'll run indefinitely
    if(shmdt(qarr)<0){perror("shmdt qarr");}
    if(shmdt(qctl)<0){perror("shmdt qctl");}

    shmctl(qarrkey, IPC_RMID, 0);
    shmctl(qctlkey, IPC_RMID, 0);
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