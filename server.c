#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/un.h>
#include "queue.h"


#define MAXEV 5
#define LPATH "myPath"
#define IPORT 8080
#define MAXCL 10

typedef struct node {
    int id;
    char *name;
} node;

int isockfd;
int lsockfd;
int clientc = 0;
Queue *pongarr;
node *pingarr;
// node *head = NULL;
// node *ping = NULL;
pthread_t timerT;
pthread_t inputT;
pthread_mutex_t pongMux;
pthread_cond_t pongMon;
pthread_cond_t clMon;
node *clientarr;


/*
NOTES
assumptions:
name packet:
    name - max 20bytes
data for client packet:
    operation - char, / * + - or @ (ping)
    result - int
    var2 - int
data from client:
    type - = result, @ pong
    result - int or 0 when pong


TIP: maybe add checksum at the end informing of string size 
*/





void cleanup(){
     //shutdown
    shutdown(isockfd, SHUT_RDWR); //ideally should wait for transfer end, but i'll do that for client
    shutdown(lsockfd, SHUT_RDWR);
    //close
    close(isockfd);
    close(lsockfd);
    unlink(LPATH);
    free(pingarr);
    free(clientarr);
    destroyQueue(pongarr);
    pthread_mutex_destroy(&pongMux);
    pthread_cond_destroy(&pongMon);
    pthread_cond_destroy(&clMon);
}

void intsig(int signum){
    cleanup();
    printf("\nreceived SIGINT\n");
    exit(EXIT_FAILURE);
}




void initclient(node *list){
    for(int i=0; i<MAXCL; i++){
        list[i].id=-1;
    }
}

int addclient(node *list, int fd, char *name){
    int id=0;
    for(int i=0; i<MAXCL; i++){
        if(list[i].id==-1){
            id=i;
            continue;
        }
        if(strcmp(list[i].name,name)){
            return -1;
        }
    }
    list[id].id=fd;
    list[id].name=name;
    return 0;
}

void delclient(node *list, int fd){
    for(int i=0; i<MAXCL; i++){
        if(list[i].id == fd){
            list[i].id=-1;
        }
    }
}

void *timerThread(void *param){
    while(1){
        char msg[50];
        memset(&msg, '0', sizeof(msg));
        sprintf(msg, "@ 0 0");
        pthread_mutex_lock(&pongMux);
        while(clientc==0){
            pthread_cond_wait(&clMon, &pongMux);
        }
        for(int i=0; i<MAXCL; i++){
            if(clientarr[i].id != -1){
                puts("pinging");
                if(send(clientarr[i].id, msg, strlen(msg)+1, 0) < 0){puts("Send failed");cleanup();exit(EXIT_FAILURE);}
                addclient(pingarr,clientarr[i].id, "");
            }
        }
        pthread_mutex_unlock(&pongMux);
        sleep(5);//wait for response
        //process response (if no response from pinged client, remove it from list and close its socket)
        //mutex on queue for waiting stuff
        pthread_mutex_lock(&pongMux);
        if(isEmpty(pongarr)){
            for(int i=0; i<MAXCL; i++){
                if(pingarr[i].id!=-1){
                    printf("closing inactive %d\n", pingarr[i].id);
                    close(pingarr[i].id);
                }
            }
        }else{
            for(int i=0; i<pongarr->size; i++){
                int fd = dequeue(pongarr);
                int found = 1;
                for(int j=0; j<MAXCL; j++){
                    if(pingarr[i].id==fd){
                        found = 0;
                    }
                }
                if(!found){
                    printf("closing inactive %d\n", fd);
                    close(fd);
                }
            }
        }
        pthread_mutex_unlock(&pongMux);
    }
    return NULL;
}

int sendcl(int lastid, node *clarr, int size, char *msg, int msgsize){
    for(int i=lastid; i<size+lastid; i++){
        if(clarr[i%size].id!=-1){
            if(send(clarr[i%size].id, msg, msgsize, 0) < 0){puts("Send failed");cleanup();exit(EXIT_FAILURE);}
            return i+1;
        }
    }
    return -1;
}

void *inputThread(void *param){
    int lastcl=0;
    while(1){
        char msg[20];
        fgets(msg, sizeof(msg), stdin);
        if(clientc>0){
            int tmp = sendcl(lastcl, clientarr, MAXCL, msg, sizeof(msg));
            if(tmp!=-1){lastcl=tmp;}else{puts("error sending");}
        }else{
            puts("no clients connected");
        }
    }
    return NULL;
}
 
int main(int argc , char *argv[]){
    signal(SIGINT, intsig);
    struct sockaddr_in iserver;
    char client_message[100];
    clientarr = malloc(MAXCL*sizeof(node));
    pingarr = malloc(MAXCL * sizeof(node));
    pongarr = createQueue(MAXCL);
    initclient(clientarr);
    initclient(pingarr);

    //Socket setup
    isockfd = socket(AF_INET , SOCK_STREAM , 0); if(isockfd == -1){printf("Could not create socket");cleanup();exit(EXIT_FAILURE);}
    puts("Internet socket created");

    lsockfd = socket(AF_LOCAL , SOCK_STREAM , 0); if(lsockfd == -1){printf("Could not create socket");cleanup();exit(EXIT_FAILURE);}
    puts("Local socket created");


    //epoll setup
    int epfd = epoll_create1(0);

    //call AFTER sock()
    static struct epoll_event ev1, ev2;
    ev1.events = EPOLLIN | EPOLLET;
    ev1.data.fd = isockfd;
    ev2.events = EPOLLIN | EPOLLET;
    ev2.data.fd = lsockfd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, isockfd, &ev1)<0){perror("eplctl");cleanup();exit(EXIT_FAILURE);}
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, lsockfd, &ev2)<0){perror("eplctl");cleanup();exit(EXIT_FAILURE);}

    //Bind
    iserver.sin_family = AF_INET;
    iserver.sin_addr.s_addr = INADDR_ANY;
    iserver.sin_port = htons( IPORT );

    struct sockaddr_un lserver;
    lserver.sun_family = AF_UNIX;
    strcpy(lserver.sun_path, LPATH);
     
    if(bind(isockfd, (struct sockaddr *)&iserver, sizeof(iserver)) < 0){perror("Internet bind failed. Error");cleanup();exit(EXIT_FAILURE);}
    puts("Internet bind");

    if(bind(lsockfd, (struct sockaddr *)&lserver, sizeof(lserver)) < 0){perror("Local bind failed. Error");cleanup();exit(EXIT_FAILURE);}
    puts("Local bind");
     
    //Listen
    listen(isockfd , 5);
    listen(lsockfd , 5);
    printf("listening on: %d %s\n", IPORT, LPATH);
    



    pthread_create(&timerT, 0, &timerThread, 0);
    pthread_create(&inputT, 0, &inputThread, 0);
    struct epoll_event events[MAXEV]; 
    //while true
    while(1){
        //wait
        int nfds = epoll_wait(epfd, events, MAXEV, 100);if(nfds<0){perror("eplwait");exit(EXIT_FAILURE);}
        //printf("events: %d\n", nfds);
        for(int i=0; i<nfds; i++){
            if(events[i].data.fd == isockfd){
                //get new socket and push it to epoll
                struct sockaddr_in clientaddr;
                socklen_t addrlen = sizeof(clientaddr);

                int conn_sock = accept(isockfd, (struct sockaddr *) &clientaddr, &addrlen);
                if(conn_sock == -1){perror("accept");cleanup();exit(EXIT_FAILURE);}
                fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFD, 0)|O_NONBLOCK);
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1){perror("epoll_ctl: conn_sock");cleanup();exit(EXIT_FAILURE);}

                puts("accept");
                char msg[50];
                memset(&msg, '\0', sizeof(msg));
                sprintf(msg, "0");
                if(send(conn_sock, msg, strlen(msg)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
                       
            }else if(events[i].data.fd == lsockfd){
                //get new socket and push it to epoll
                //get new socket and push it to epoll
                struct sockaddr_in clientaddr;
                socklen_t addrlen = sizeof(clientaddr);

                int conn_sock = accept(lsockfd, (struct sockaddr *) &clientaddr, &addrlen);
                if(conn_sock == -1){perror("accept");cleanup();exit(EXIT_FAILURE);}
                fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFD, 0)|O_NONBLOCK);
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1){perror("epoll_ctl: conn_sock");cleanup();exit(EXIT_FAILURE);}

                puts("accept");
                char msg[50];
                memset(&msg, '\0', sizeof(msg));
                sprintf(msg, "0");
                if(send(conn_sock, msg, strlen(msg)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }else if(events[i].events & EPOLLIN){
                printf("data received\n");
                int n = recv(events[i].data.fd, client_message, 100, 0);
                if(n == 0){
                    //remove client from list
                    pthread_mutex_lock(&pongMux);
                    delclient(clientarr, events[i].data.fd);
                    clientc--;
                    pthread_mutex_unlock(&pongMux);
                    printf("client closed connection\n");
                }
                if(client_message[0] != '=' && client_message[0] != '@'){ //register
                    pthread_mutex_lock(&pongMux);
                    if(addclient(clientarr, events[i].data.fd, client_message)!=-1){
                        clientc++;
                        pthread_cond_broadcast(&clMon);
                    }
                    pthread_mutex_unlock(&pongMux);
                }
                if(client_message[0] == '@'){
                    //lock mutex
                    pthread_mutex_lock(&pongMux);
                    //add to special list
                    enqueue(pongarr, events[i].data.fd);
                    //broadcast mon
                    pthread_cond_broadcast(&pongMon);
                    //unlock
                    pthread_mutex_unlock(&pongMux);
                }
                printf("rec: len:%d msg:%s\n", n, client_message);
                // if(n!=0){
                //     puts("sending eq");
                //     char msg[50];
                //     memset(&msg, '\0', sizeof(msg));
                //     sprintf(msg, "* 23 45");
                //     if(send(events[i].data.fd, msg, strlen(msg)+1, 0) < 0){printf("Send failed %d\n",__LINE__);perror("send");cleanup();exit(EXIT_FAILURE);}
                // }
            }else{
                puts("unknown epoll event");
            }
            printf("ev %d\n", events[i].data.fd);
        }
        sleep(1); //for dbg, to prevent flooding with messages
    }
}