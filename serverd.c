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


int isockfd;
int lsockfd;
int clientc = 0;

struct sockaddr clients[MAXCL];

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
}

void intsig(int signum){
    cleanup();
    printf("\nreceived SIGINT\n");
    exit(EXIT_FAILURE);
}



void *inputThread(void *param){
    int lastcl=0;
    while(1){
        char msg[20];
        fgets(msg, sizeof(msg), stdin);
        if(clientc>0){
            sendto(isockfd, msg, sizeof(msg), 0, &clients[lastcl], sizeof(clients[lastcl]));
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



    //Socket setup
    isockfd = socket(AF_INET , SOCK_DGRAM , 0); if(isockfd == -1){printf("Could not create socket");cleanup();exit(EXIT_FAILURE);}
    puts("Internet socket created");

    lsockfd = socket(AF_LOCAL , SOCK_DGRAM , 0); if(lsockfd == -1){printf("Could not create socket");cleanup();exit(EXIT_FAILURE);}
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



    struct epoll_event events[MAXEV]; 
    //while true
    while(1){
        //wait
        int nfds = epoll_wait(epfd, events, MAXEV, 100);if(nfds<0){perror("eplwait");exit(EXIT_FAILURE);}
        //printf("events: %d\n", nfds);
        for(int i=0; i<nfds; i++){
            if(events[i].data.fd == isockfd){
                // get new socket and push it to epoll
                // struct sockaddr_in clientaddr;
                // socklen_t addrlen = sizeof(clientaddr);

                // int conn_sock = accept(isockfd, (struct sockaddr *) &clientaddr, &addrlen);
                // if(conn_sock == -1){perror("accept");cleanup();exit(EXIT_FAILURE);}
                // fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFD, 0)|O_NONBLOCK);
                // struct epoll_event ev;
                // ev.events = EPOLLIN | EPOLLET;
                // ev.data.fd = conn_sock;
                // if(epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1){perror("epoll_ctl: conn_sock");cleanup();exit(EXIT_FAILURE);}

                // puts("accept");
                // char msg[50];
                // memset(&msg, '\0', sizeof(msg));
                // sprintf(msg, "0");
                // if(send(conn_sock, msg, strlen(msg)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
                if(clientc<MAXCL){
                    recvfrom(isockfd, client_message, sizeof(client_message), 0, &clients[clientc], (socklen_t *)sizeof(clients[clientc]));
                    clientc++;
                    printf("recv: %s\n", client_message); 
                }
            }else if(events[i].data.fd == lsockfd){
                //get new socket and push it to epoll
                recvfrom(isockfd, client_message, sizeof(client_message), 0, NULL, NULL);
                printf("recv: %s\n", client_message); 

            }else{
                puts("unknown epoll event");
            }
            printf("ev %d\n", events[i].data.fd);
        }
        sleep(1); //for dbg, to prevent flooding with messages
    }
}