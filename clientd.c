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
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>


#define CLNAME "iwan" //max 20 bytes

int sockfd;

void intsig(int signum){
    //shutdown - unregister
    shutdown(sockfd, SHUT_RDWR);
    //close
    close(sockfd);
    printf("\nreceived SIGINT\n");
    exit(EXIT_FAILURE);
}

void cleanup(){
     //shutdown
    shutdown(sockfd, SHUT_RDWR); //ideally should wait for transfer end, but i'll do that for client
    close(sockfd);
}

int main(int argc , char *argv[]){
    signal(SIGINT, intsig);
    struct sockaddr_in server;
    char message[100] , server_reply[100];
    if(argc != 3){puts("wrong params");exit(EXIT_FAILURE);}

    if(argv[1][0] == '0'){
        //Create socket
        sockfd = socket(AF_INET , SOCK_STREAM , 0);if(sockfd==-1){printf("Could not create socket");perror("socket");exit(EXIT_FAILURE);}
        puts("Internet socket created");

        //fill up  sockaddr struct using some function i dont rmemember to get data
        memset(&server, '0', sizeof(server));
        server.sin_addr.s_addr = inet_addr("127.0.0.1"); //should be param, gethostbyname();
        server.sin_family  = AF_INET;
        server.sin_port = htons( 8080 );
        //memset(sendBuff, '0', sizeof(sendBuff));
    
        memset(&server_reply, '0', sizeof(server_reply)); //safety measure
        //Connect to remote server
        if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
        puts("Connected\n");
    }else if(argv[1][0] == '1'){
        //Create socket
        sockfd = socket(AF_LOCAL, SOCK_STREAM , 0);if(sockfd==-1){printf("Could not create socket");perror("socket");exit(EXIT_FAILURE);}
        puts("Local socket created");

        //fill up  sockaddr struct using some function i dont rmemember to get data
        struct sockaddr_un lserver;
        lserver.sun_family = AF_UNIX;
        strcpy(lserver.sun_path, argv[2]);
    
        memset(&server_reply, '0', sizeof(server_reply)); //safety measure
        //Connect to remote server
        if(connect(sockfd, (struct sockaddr *)&lserver, sizeof(lserver)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
        puts("Connected\n");
    }else{
        puts("unknown mode");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    
    //send client name
    if(send(sockfd, CLNAME, sizeof(CLNAME), 0) < 0){puts("Send failed");cleanup();exit(EXIT_FAILURE);}
    printf("sending name: %s\n", CLNAME);

    //check if server accepted
    if(recv(sockfd , server_reply , 20 , 0) < 0){puts("recv failed");cleanup(); exit(EXIT_FAILURE);}
    if('0' == server_reply[0]){puts("client accepted");}else{puts("client exists, exiting");cleanup();exit(EXIT_SUCCESS);}

    //keep communicating with server
    while(1){
        //Receive a reply from the server, blocking, client does not do anything else than processing stuff from server
        int n;
        if((n = recv(sockfd, server_reply, 50, 0)) < 0){puts("recv failed");cleanup();exit(EXIT_FAILURE);}
        if(n == 0){puts("server closed connection");cleanup();exit(EXIT_SUCCESS);}
        printf("received: %s\n", server_reply);
        // printf("Enter message : ");
        // scanf("%s" , message);
        char mode;
        int var1;
        int var2;
        sscanf(server_reply, "%c %d %d", &mode, &var1, &var2);

        /// * + - or = or @ (ping)
        switch(mode){
            case '*' :{
                int res = var1 * var2;
                memset(&message, '0', sizeof(message));
                sprintf(message, "= %d", res);
                printf("sending %d\n", res);
                if(send(sockfd, message, strlen(message)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }break;

            case '+' :{
                int res = var1 + var2;
                memset(&message, '0', sizeof(message));
                sprintf(message, "= %d", res);
                printf("sending %d\n", res);
                if(send(sockfd, message, strlen(message)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }break;

            case '-' :{
                int res = var1 - var2;
                memset(&message, '0', sizeof(message));
                sprintf(message, "= %d", res);
                printf("sending %d\n", res);
                if(send(sockfd, message, strlen(message)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }break;

            case '/' :{
                int res = var1 / var2;
                memset(&message, '0', sizeof(message));
                sprintf(message, "= %d", res);
                printf("sending %d\n", res);
                if(send(sockfd, message, strlen(message)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }break;

            case '@' :{
                //just pong
                memset(&message, '0', sizeof(message));
                sprintf(message, "@ %d", 0);
                printf("sending pong\n");
                if(send(sockfd, message, strlen(message)+1, 0) < 0){printf("Send failed %d\n",__LINE__);cleanup();exit(EXIT_FAILURE);}
            }break;

            default :{
                printf("unknown operation: %c\n", mode);
                printf("message: %s\n", server_reply);
            }
        }
    }
}