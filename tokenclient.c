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
#include <errno.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


#define MAXRT 3 //maximum allowed retries before giving up
#define RTTMR 500 //time after which retry


/*
Token ring:
Empty information frames are continuously circulated on the ring.
When a computer has a message to send, it seizes the token. The computer will then be able to send the frame.
The frame is then examined by each successive workstation. The workstation that identifies itself to be the destination for the message copies it from the frame and changes the token back to 0.
When the frame gets back to the originator, it sees that the token has been changed to 0 and that the message has been copied and received. It removes the message from the frame.
The frame continues to circulate as an "empty" frame, ready to be taken by a workstation when it has a message to send.

 particular timer on an end station expires such as the case when a station hasn't seen a token frame in the past 7 seconds.
When any of the above conditions take place and a station decides that a new monitor is needed, it will transmit a "claim token" frame, 
announcing that it wants to become the new monitor. If that token returns to the sender, it is OK for it to become the monitor. 
*/

/*
when one client sends the message, it passess whole ring and should by this time be received,
the message should contain information from who it is and retranssmision counter.
after two unsuccessful tries, the sender after receiving the message clears the token

loggers will be in python, probably through udp

connecting new clients? - 
maybe somehow send the message with new address to reconnect client
no, wont work since new client would need to have token

preventing token duplicate/removal - 
maybe after sending wait n seconds and if nothing arrives by then try again

duplicates - sender marks the token somehow everytime it passess, like say
i write 1, wait for token with one, immediately change to two, discard any token which doesnt match

ring break - no implementation, just idea
if the client holding the token would disconnect, leaving the ring without one node AND token
the other clients would start retransmitting as follows
after some time (+random) all clients transmit their address to the next, unless they received the message
if client receives the message, then passess it further,
every client remembers the last address that arrived
if their own didnt return (link broken) it sends to the last which it remembers (link fixed)
and that token stays as new token
*/


int insockfd;
int outsockfd;
int logsockfd;
int prevfd = 0;

void cleanup(){
    //shutdown - unregister
    shutdown(insockfd, SHUT_RDWR);
    shutdown(outsockfd, SHUT_RDWR);
    shutdown(prevfd, SHUT_RDWR);
    shutdown(logsockfd, SHUT_RDWR);
    //close
    close(insockfd);
    close(outsockfd);
    close(prevfd);
    close(logsockfd);
}


void intsig(int signum){
    cleanup();
    printf("\nreceived SIGINT\n");
    exit(EXIT_FAILURE);
}



/*
header structure:
to include, but not neccessarily in order:

message type 1B - for new clients and stuff 
    0 - empty
    1 - message
    2 - message read
    3 - getaddr (write self address inside unless sender) 
        !it cant be the address since we recognize recipients by nickname so lets use that
    4 - receiving adds the sender as nextnode

retransmission counter - 1B (or maybe even 1b), flag if the message was received, if it was not will be unchanged or smth
sender address (or nickname?) - rather ip
receiver nickname
signature (for duplicate packet removal)
text

format:
or maybe ignore chars and send bits and bytes
type[char 1] rtc[char 1] recipient[char 10] sender_ip[char 16] sender_port[char 4] |=37 bytes, lets say 1500 for text
*/


/*
Params:

+1:tekstowy identyfikator użytkownika, char[10]
-2:port na którym ma słuchać
-3:adres IP sasiada
-4:port sąsiada, do którego przekazywane będą wiadomości
-5:informacja o tym, czy dany użytkownik po uruchomieniu posiada token, 1/0
+6:wybrany protokół: tcp lub udp. t/u
*/

long dec(char *num){
    return strtol(num, NULL, 10);
}

// TODO: add duplicate removal

//start:    ./tokenclient jacek 1111 first 1112 1 t
//          ./tokenclient janek 1112 localhost 1111 0 t
//          ./tokenclient placek 1113 localhost 1111 0 t
int main(int argc , char *argv[]){
    signal(SIGINT, intsig);
    struct sockaddr_in nextnode, iserver, prevnode, lserver, lognode, tempnode; //nextnode is next client, iserver is instace's listening socket for accepting new clients only
    socklen_t addrlen = sizeof(iserver);
    char server_reply[1540], packet[1540]; //max text message size: 1501 (for nullbyte)
    char nickname[10];
    struct timeval timeout;      
    timeout.tv_sec = 1;
    timeout.tv_usec = 5000;
    in_addr_t tempaddr;
    int tempport;
    clock_t retransmission_timer = 0;
    char log_ip[] = "226.1.1.5";


    if(argc != 7){puts("wrong number of parameters, expecting:");puts("nickname port nextip nextport token protocol");exit(EXIT_FAILURE);}

    srand(time(NULL));

    //setup self name
    memset(&nickname, '\0', sizeof(nickname));
    memcpy(&nickname, argv[1], sizeof(nickname));
    printf(ANSI_COLOR_CYAN"nick: %s\n"ANSI_COLOR_RESET, nickname); //debug
    
    int firstnode = 0, connecting_new = 0, first_token = argv[5][0]-'0';
    int this_port = dec(argv[2]);
    in_addr_t this_ip = INADDR_ANY;

    //setup logger sock
    logsockfd = socket(AF_INET , SOCK_DGRAM , 0);if(logsockfd==-1){printf(ANSI_COLOR_RED"Could not create log socket\n"ANSI_COLOR_RESET);perror("socket");exit(EXIT_FAILURE);}
    memset((char *) &lognode, 0, sizeof(lognode));
    lserver.sin_family = AF_INET;
    lserver.sin_addr.s_addr = INADDR_ANY;
    lserver.sin_port = 0; //get auto port
    if(bind(logsockfd, (struct sockaddr *)&lserver, sizeof(lserver)) < 0){perror("logbind failed. Error");cleanup();exit(EXIT_FAILURE);}

    //setup receiving address
    lognode.sin_family = AF_INET;
    lognode.sin_addr.s_addr = inet_addr(log_ip);
    lognode.sin_port = htons(4321);

    if(argv[6][0] == 't'){
        //Create tcp socket
        insockfd = socket(AF_INET , SOCK_STREAM , 0);if(insockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
        outsockfd = socket(AF_INET , SOCK_STREAM , 0);if(outsockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
        puts("Internet sockets created"); //debug
        
        
        //make the socket reuse address and nonblocking
        if (setsockopt (outsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){ perror("setsockopt failed\n"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (outsockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){perror("setsockopt(SO_REUSEADDR) failed"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (insockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){ perror("setsockopt failed\n"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (insockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){perror("setsockopt(SO_REUSEADDR) failed"); cleanup();exit(EXIT_FAILURE);}


        //Bind listening socket
        iserver.sin_family = AF_INET;
        iserver.sin_addr.s_addr = this_ip;
        iserver.sin_port = htons( this_port ); //listen on given port
    
        //safety measure
        memset(&server_reply, '\0', sizeof(server_reply)); 
        memset(&prevnode, '\0', sizeof(prevnode));
        memset(&nextnode, '\0', sizeof(nextnode));

        //prepare socket for incoming connections
        if(bind(insockfd, (struct sockaddr *)&iserver, sizeof(iserver)) < 0){perror("Internet bind failed. Error");cleanup();exit(EXIT_FAILURE);}
        listen(insockfd , 5);
        printf("Listening\n");

        //if nextnode not provided wait for new clients, otherwise connect
        if(strncmp(argv[3], "first", 5) == 0){
            //since i have no way to accept connection and make it at the same time without threads i'll wait for clients
            //this flag will let the current client know that it should set it's nextnode pointer to the first connecting clients
            printf(ANSI_COLOR_CYAN"firstnode\n"ANSI_COLOR_RESET); //debug
            firstnode = 1;
        }else if(strncmp(argv[3], "localhost", 9) == 0){
            memset(&nextnode, '0', sizeof(nextnode));
            nextnode.sin_addr.s_addr = this_ip;
            nextnode.sin_family  = AF_INET;
            nextnode.sin_port = htons( dec(argv[4]) ); //convert from cmdline
            if(connect(outsockfd, (struct sockaddr *)&nextnode, sizeof(nextnode)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
            printf(ANSI_COLOR_GREEN"Connected local\n"ANSI_COLOR_RESET);
            char msg[50];
            sprintf(msg, "%u %d", this_ip, this_port);
            if(send(outsockfd, msg, sizeof(msg), 0) < 0){perror("firstsend:Send failed");}
            printf("address sent\n");
        }else{
            //setup the socket and connect
            memset(&nextnode, '0', sizeof(nextnode));
            nextnode.sin_addr.s_addr = inet_addr(argv[3]); //should be supplied
            nextnode.sin_family  = AF_INET;
            nextnode.sin_port = htons( dec(argv[4]) ); //convert from cmdline
            if(connect(outsockfd, (struct sockaddr *)&nextnode, sizeof(nextnode)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
            printf(ANSI_COLOR_GREEN"Connected\n"ANSI_COLOR_RESET);
            printf(ANSI_COLOR_GREEN"Connected local\n"ANSI_COLOR_RESET);
            char msg[50];
            sprintf(msg, "%u %d", this_ip, this_port);
            if(send(outsockfd, msg, sizeof(msg), 0) < 0){perror("firstsend:Send failed");}
            printf("address sent\n");
        }
        
        
        

        int tempfd = 0;
        while(1){
            //accept connections if new clients want to join, this fd will be used for receiving token
            //also this struct will give me their address
            if(!connecting_new){ //so i won't override the current fd
                tempfd = accept(insockfd, (struct sockaddr *)&prevnode, &addrlen);            
            
                //before the next two blocks execute do the errno check
                if(errno != EAGAIN && errno != EWOULDBLOCK){
                    printf(ANSI_COLOR_CYAN"incoming connection\n"ANSI_COLOR_RESET); //debug
                    if(firstnode){
                        //the previous node is at the same time the next node
                        //but since its listening on a different port we have to recv first
                        char msg[50];
                        int recvcode = recv(tempfd, &msg, sizeof(msg), 0);if(recvcode<0){perror("recv conn failed");cleanup(); exit(EXIT_FAILURE);}
                        if(errno != 0){perror("error recv");}
                        in_addr_t nextaddr;
                        int nextport;
                        sscanf(msg, "%u %d", &nextaddr, &nextport);
                        printf("port %d ip %u\n", nextport, nextaddr);
                        nextnode.sin_family = AF_INET;
                        nextnode.sin_addr.s_addr = nextaddr;
                        nextnode.sin_port = htons(nextport);
                        if(connect(outsockfd, (struct sockaddr *)&nextnode, sizeof(nextnode)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
                        printf(ANSI_COLOR_GREEN"added node\n"ANSI_COLOR_RESET);
                        //new node will send request to be joined but since there is two of them, the 1st node already knows that and will forward token
                        firstnode = 0;
                    }

                    //when the node doesnt have previous just accept connection since its the ring joining
                    //otherwise we have new client and need to process acoordingly
                    if(prevfd == 0){
                        prevfd = tempfd;
                    }else{
                        //see below on how to proceed when joins
                        /*
                        basically:
                        no need for recv, we have the address from struct

                        as below, we wait for token
                        when token arrives, we poll for the nickname, option 3, next time we get token we know the previous node's nick 
                            (actually we can use the saved one, but above will be good for udp)
                                so, lets use the saved one 
                                    but we work on nicknames so we want the previous nickname, not the address
                        now we know to who to talk, we send the new node's address to the previous nick
                        -previous node joins
                        possibly need to terminate connection first
                        prevfd = tempfd;
                        */
                    
                        char msg[50];
                        int recvcode = recv(tempfd, &msg, sizeof(msg), 0);if(recvcode<0){perror("recv conn failed");cleanup(); exit(EXIT_FAILURE);}
                        if(errno != 0){perror("error recv");}
                        sscanf(msg, "%u %d", &tempaddr, &tempport);
                        printf("client %d ip %u\n", tempport, tempaddr);
                    //to keep it stateless (as much as possible) we'll handel this in the recv part
                        connecting_new = 1;

                    /*
                    new connection should get addr from tcp
                        receiving client remembers it needs to add client, waits for empty token
                        when token arrives, it sends special message, needing all the clients to supply their id (everyone replaces it with own)
                        when it makes full circle, the sender knows who is before it, sends second message with that address and the new clients address
                        when the receiving client gets the message, it changes his nextnode variable to supplied address
                    */
                    }
                }else{ 
                    errno = 0;
                    printf("no conn\n"); //debug
                }
            }
            
            //if has token and is not the only node
            if(first_token && !firstnode){
                printf(ANSI_COLOR_CYAN"sending first token\n"ANSI_COLOR_RESET);
                memset(&packet, '\0', sizeof(packet));
                sprintf(packet, "%d %d %s %u %d %s", 0, 0, nickname, this_ip, this_port, "hello");
                if(send(outsockfd, packet, sizeof(packet), 0) < 0){perror("firstsend:Send failed");}
                retransmission_timer = clock();
                first_token = 0;
            }
            
            //if token didn't return after given time, try again
            printf("clocks: %ld\n", clock()-retransmission_timer);
            if(retransmission_timer != 0 && clock()-retransmission_timer >= RTTMR){
                printf(ANSI_COLOR_RED"retransmitting token\n"ANSI_COLOR_RESET);
                if(send(outsockfd, packet, sizeof(packet), 0) < 0){perror("retransmission failed");}
            }




            //regular recv, waiting for token
            int recvcode = recv(prevfd, &server_reply, sizeof(server_reply), 0); 
            //check if we got the message
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                //hopefully won't interrupt the transmission
                if(recvcode == -1){
                    perror("recv failed");
                }else if(recvcode == 0){
                    //previous node terminated, need to rearrange ring (if it would exit cleanly the ring wouldn't know its missing or smth)
                    //not required, won't be implemented because of  time constraints
                    /*
                    how it could work:
                    since its tcp we know that connection terminated
                    the two affected nodes will know that 
                    the one that had it as incoming will send a special packet for everyone to inspect with its own address
                    the second one which had missing one as nextnode on receiving that address will swap to it
                    ring fixed
                    */
                    if(connecting_new){
                        prevfd = tempfd;
                        connecting_new = 0;
                    }
                   printf(ANSI_COLOR_MAGENTA"conn closed\n"ANSI_COLOR_RESET);
                }else{
                    retransmission_timer = 0;
                    //token received, process and send
                    printf("received %d bytes\n", recvcode);
                    //only sending char[10] nickname to logger
                    

                    sleep(rand()%(int)3 + 1); //debug, for simulation purposes

                    /*message type 1B - for new clients and stuff 
                        0 - empty
                        1 - message
                        2 - message read
                        3 - getaddr (write self address inside unless sender) 
                            !it cant be the address since we recognize recipients by nickname so lets use that
                        4 - receiving adds the sender as nextnode
                        5 - receiver succesfully connected

                        type[char 1] rtc[char 1] recipient[char 10] sender_ip[char 4] sender_port[char 4] |=25 bytes, lets say 1500 for text
                    */
                    //!it should be stateless

                    /*
                    first inspect the type
                        0 - if client wants to send, it fills the fields
                        1 - checks the recipient, if self reads the text part, changes type to 2 and sends further
                        2 - if sender == this client, clear the message, else send further
                        3 - if sender != self, write self to recipient
                        4 - if recipient == self, add the sender as nextnode
                            sender is combo ip+port
                    */

                    int type;
                    int rtc;
                    char recp[10];
                    in_addr_t sender_ip; //not sure if thats how it works
                    int sender_port;
                    char text[1500];
                    memset(&sender_ip, '\0', sizeof(sender_ip));
                    memset(&recp, '\0', sizeof(recp));
                    memset(&text, '\0', sizeof(text));


                    char logpacket[15];
                    memset(&logpacket, '\0', sizeof(logpacket));
                    sprintf(logpacket, "%s %d", nickname, type);
                    if(sendto(logsockfd, logpacket, sizeof(logpacket), 0, (struct sockaddr*)&lognode, sizeof(lognode)) < 0){perror("logpacket: Send failed");}

                    printf("packet: %s\n", server_reply);
                    sscanf(server_reply, "%d %d %s %u %d %s", &type, &rtc, recp, &sender_ip, &sender_port, text); //how to work with header
                    memset(server_reply, '\0', sizeof(server_reply));
                    switch(type){
                        case 0:
                            if(!connecting_new){                         
                                //when sending, first memset the packet to \0
                                memset(&packet, '\0', sizeof(packet));

                                
                                //replace the text with random message
                                char message[] = "random text message";
                                //send to hardcoded test recipient
                                sprintf(packet, "%d %d %s %u %d %s", 1, 0, "jacek", this_ip, this_port, message);

                                printf("sending token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("0:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //we need to obtain prevnode nickname so we send the 3-type message
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 3, 0, nickname, this_ip, this_port, text);
                                printf(ANSI_COLOR_CYAN"Polling name token\n"ANSI_COLOR_RESET);
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("0:3:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 1:
                            //! remember to check retransimssion
                            if(strncmp(recp, nickname, 10) == 0){
                                //change type to 2 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 2, rtc, recp, sender_ip, sender_port, text);
                                printf("replying token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("1:Send failed");}
                                retransmission_timer = clock();
                            }else if(sender_port == this_port){
                                if(rtc < MAXRT){
                                    memset(&packet, '\0', sizeof(packet));
                                    rtc++;
                                    sprintf(packet, "%d %d %s %u %d %s", type, rtc, recp, sender_ip, sender_port, text);
                                    printf(ANSI_COLOR_YELLOW"token circled\n"ANSI_COLOR_RESET);
                                    if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("1:rt Send failed");}
                                    retransmission_timer = clock();
                                }else{
                                    memset(&packet, '\0', sizeof(packet));
                                    rtc++;
                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("clearing and sending token\n");
                                    if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("1:rtc0 Send failed");}
                                    retransmission_timer = clock();
                                }
                            }else{
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("1:rtc0 Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 2:
                            //for the sake of simplicity ill just compare the ports, since its localhost
                            //otherwise id need to check the machine's ip in network
                            if(sender_port == this_port){
                                //change type to 0 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                printf("accepting token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("2:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //change type to 0 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", type, rtc, recp, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("2:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 3:
                            if(sender_port != this_port){
                                //replace the recipient with own data and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", type, rtc, nickname, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("3:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //change type to 4, fill properly and send
                                memset(&packet, '\0', sizeof(packet));
                                //change the sender ip and port to the one of new client and send
                                sprintf(packet, "%d %d %s %u %d %s", 4, 0, recp, tempaddr, tempport, text);
                                printf(ANSI_COLOR_CYAN"newnode token\n"ANSI_COLOR_RESET);
                                if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("3:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 4:
                            if(strncmp(recp, nickname, 10) == 0){
                                if(nextnode.sin_addr.s_addr != sender_ip || nextnode.sin_port != sender_port){
                                    memset(&packet, '\0', sizeof(packet));
                                    //change nextnode to the one supplied
                                    shutdown(outsockfd, SHUT_RDWR);
                                    close(outsockfd);
                                    //extracted from packet
                                    nextnode.sin_family = AF_INET;
                                    nextnode.sin_addr.s_addr = sender_ip;
                                    nextnode.sin_port = htons(sender_port);
                                    outsockfd = socket(AF_INET , SOCK_STREAM , 0);if(outsockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
                                    if(connect(outsockfd, (struct sockaddr *)&nextnode, sizeof(nextnode)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
                                    printf(ANSI_COLOR_GREEN"connected new node\n"ANSI_COLOR_RESET);

                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("sending token\n");
                                    if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("4:Send failed");}
                                    retransmission_timer = clock();
                                }else{
                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("sending token\n");
                                    if(send(outsockfd, packet, sizeof(packet), 0) < 0){puts("4:Send failed");}
                                    retransmission_timer = clock();
                                }
                            }
                            break;                            
                        default:
                            //malformed token, clear?
                            break;
                    }
                }
            }else{
                //the flag needs to be reset
                printf("no new message\n"); //debug
                errno = 0;
            }
        }

        //end tcp
    }else if(argv[6][0] == 'u'){
        //create udp socket
        
        insockfd = socket(AF_INET , SOCK_DGRAM , 0);if(insockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
        outsockfd = socket(AF_INET , SOCK_DGRAM , 0);if(outsockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
        printf(ANSI_COLOR_GREEN"Internet dgram sockets created\n"ANSI_COLOR_RESET); //debug
        
        
        //make the socket reuse address and nonblocking
        if (setsockopt (outsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){ perror("setsockopt failed\n"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (outsockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){perror("setsockopt(SO_REUSEADDR) failed"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (insockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){ perror("setsockopt failed\n"); cleanup();exit(EXIT_FAILURE);}
        if (setsockopt (insockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){perror("setsockopt(SO_REUSEADDR) failed"); cleanup();exit(EXIT_FAILURE);}


        //Bind listening socket
        iserver.sin_family = AF_INET;
        iserver.sin_addr.s_addr = this_ip;
        iserver.sin_port = htons( this_port ); //listen on given port
    
        //safety measure
        memset(&server_reply, '\0', sizeof(server_reply)); 
        memset(&prevnode, '\0', sizeof(prevnode));
        memset(&nextnode, '\0', sizeof(nextnode));


        /*
        recvfrom saves address
        sendto uses saved address
        server: socket -> bind -> sendtorecvfrom
        client: socket -> sendto/recvfrom
        */
        //prepare socket for incoming connections
        if(bind(insockfd, (struct sockaddr *)&iserver, sizeof(iserver)) < 0){perror("Internet bind failed. Error");cleanup();exit(EXIT_FAILURE);}
        printf("Socket bound\n");

        //if nextnode not provided wait for new clients, otherwise connect
        if(strncmp(argv[3], "first", 5) == 0){
            //since i have no way to accept connection and make it at the same time without threads i'll wait for clients
            //this flag will let the current client know that it should set it's nextnode pointer to the first connecting clients
            printf(ANSI_COLOR_CYAN"firstnode\n"ANSI_COLOR_RESET); //debug
            firstnode = 1;
        }else if(strncmp(argv[3], "localhost", 9) == 0){
            memset(&nextnode, '0', sizeof(nextnode));
            nextnode.sin_addr.s_addr = this_ip;
            nextnode.sin_family  = AF_INET;
            nextnode.sin_port = htons( dec(argv[4]) ); //convert from cmdline

            // if(connect(outsockfd, (struct sockaddr *)&nextnode, sizeof(nextnode)) < 0){perror("connect failed. Error");cleanup();exit(EXIT_FAILURE);}
            // printf(ANSI_COLOR_GREEN"Transmitting local\n"ANSI_COLOR_RESET);
            sprintf(packet, "%u %d", this_ip, this_port);
            if(sendto(outsockfd, packet, 20, 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){perror("firstconn:Send failed");}
            retransmission_timer = clock();
            printf(ANSI_COLOR_GREEN"address sent\n"ANSI_COLOR_RESET);
        }else{
            //setup the socket and connect
            memset(&nextnode, '0', sizeof(nextnode));
            nextnode.sin_addr.s_addr = inet_addr(argv[3]); //should be supplied
            nextnode.sin_family  = AF_INET;
            nextnode.sin_port = htons( dec(argv[4]) ); //convert from cmdline
            
            sprintf(packet, "%u %d\n", this_ip, this_port);
            if(sendto(outsockfd, packet, 20, 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){perror("firstconn:Send failed");}
            retransmission_timer = clock();
            printf(ANSI_COLOR_GREEN"address sent\n"ANSI_COLOR_RESET);
        }

        
        //udp: from here continue modifying, things needing change won't be minimized
        //udp will only have recv to temporary structure, if address is the same proceed as usual, if mismatch, connect new client
        while(1){           
            //if has token and is not the only node
            if(first_token && !firstnode){
                printf(ANSI_COLOR_CYAN"sending first token\n"ANSI_COLOR_RESET);
                memset(&packet, '\0', sizeof(packet));
                sprintf(packet, "%u %d", this_ip, this_port);
                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){perror("firstsend:Send failed");}
                retransmission_timer = clock();
                first_token = 0;
            }
            
            //if token didn't return after given time, try again
            printf("clocks: %ld\n", clock()-retransmission_timer);
            if(retransmission_timer != 0 && clock()-retransmission_timer >= RTTMR){
                printf(ANSI_COLOR_RED"retransmitting token\n"ANSI_COLOR_RESET);
                if(sendto(outsockfd, packet, sizeof(packet), 0,(struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){perror("retransmission failed");}
            }



            
            //regular recv, waiting for token
            int recvcode = recvfrom(insockfd, &server_reply, sizeof(server_reply), 0, (struct sockaddr*)&tempnode, &addrlen); 
            printf("%d:port %d:ip\n", tempnode.sin_port, tempnode.sin_addr.s_addr);
            //check if we got the message
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                //hopefully won't interrupt the transmission
                if(recvcode == -1){
                    perror("recv failed");
                }else if(recvcode == 0){ //may need adjusting
                    //previous node terminated, need to rearrange ring (if it would exit cleanly the ring wouldn't know its missing or smth)
                    //not required, won't be implemented because of  time constraints
                    /*
                    how it could work:
                    since its tcp we know that connection terminated
                    the two affected nodes will know that 
                    the one that had it as incoming will send a special packet for everyone to inspect with its own address
                    the second one which had missing one as nextnode on receiving that address will swap to it
                    ring fixed
                    */
                   printf(ANSI_COLOR_MAGENTA"conn closed\n"ANSI_COLOR_RESET);
                // }else if(prevnode.sin_port == -1 && !firstnode){
                //     printf("saving prevnode\n");
                //     prevnode.sin_family = AF_INET;
                //     prevnode.sin_port = tempnode.sin_port;
                //     prevnode.sin_addr.s_addr = tempnode.sin_addr.s_addr;
                //     firstnode = 0;
                //     printf("sending token\n");
                //     if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("0:Send failed");}
                //     retransmission_timer = clock();
                }else if(prevnode.sin_addr.s_addr != tempnode.sin_addr.s_addr || prevnode.sin_port != tempnode.sin_port){ //connection from new address -> new client

                    //connecting new socket
                    printf(ANSI_COLOR_CYAN"incoming connection\n"ANSI_COLOR_RESET);
                    printf("conn packet: %s\n", server_reply);

                    sscanf(server_reply, "%u %d\n", &tempaddr, &tempport);
                    printf("client %d ip %u\n", tempport, tempaddr);
                    //to keep it stateless (as much as possible) we'll handle this in the recv part
                    memset(packet, '\0', sizeof(packet));
                    if(tempaddr != nextnode.sin_addr.s_addr || htons(tempport) != nextnode.sin_port){
                        nextnode.sin_family = AF_INET;
                        nextnode.sin_addr.s_addr = tempaddr;
                        nextnode.sin_port = htons(tempport);
                        firstnode = 0;
                    }
                    printf(ANSI_COLOR_GREEN"ring closed\n"ANSI_COLOR_RESET);
                    sprintf(packet, "%d %d %s %u %d %s", 0, 0, "jacek", this_ip, this_port, "");
                    printf("sending token\n");
                    if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){perror("ringconn:Send failed");}
                    //extract ip and port (different from tempnode since its previous prevnode not nextnode)
                }else{
                    retransmission_timer = 0;
                    //token received, process and send
                    printf("received %d bytes\n", recvcode);
                    //only sending char[10] nickname to logger

                    sleep(rand()%(int)3 + 1); //debug, for simulation purposes

                    /*message type 1B - for new clients and stuff 
                        0 - empty
                        1 - message
                        2 - message read
                        3 - getaddr (write self address inside unless sender) 
                            !it cant be the address since we recognize recipients by nickname so lets use that
                        4 - receiving adds the sender as nextnode
                        5 - receiver succesfully connected

                        type[char 1] rtc[char 1] recipient[char 10] sender_ip[char 4] sender_port[char 4] |=25 bytes, lets say 1500 for text
                    */
                    //!it should be stateless

                    /*
                    first inspect the type
                        0 - if client wants to send, it fills the fields
                        1 - checks the recipient, if self reads the text part, changes type to 2 and sends further
                        2 - if sender == this client, clear the message, else send further
                        3 - if sender != self, write self to recipient
                        4 - if recipient == self, add the sender as nextnode
                            sender is combo ip+port
                    */

                    int type;
                    int rtc;
                    char recp[10];
                    in_addr_t sender_ip; //not sure if thats how it works
                    int sender_port;
                    char text[1500];
                    memset(&sender_ip, '\0', sizeof(sender_ip));
                    memset(&recp, '\0', sizeof(recp));
                    memset(&text, '\0', sizeof(text));


                    char logpacket[15];
                    memset(&logpacket, '\0', sizeof(logpacket));
                    sprintf(logpacket, "%s %d", nickname, type);
                    if(sendto(logsockfd, logpacket, sizeof(logpacket), 0, (struct sockaddr*)&lognode, sizeof(lognode)) < 0){perror("logpacket: Send failed");}

                    printf("packet: %s\n", server_reply);
                    sscanf(server_reply, "%d %d %s %u %d %s", &type, &rtc, recp, &sender_ip, &sender_port, text); //how to work with header
                    memset(server_reply, '\0', sizeof(server_reply));
                    switch(type){
                        case 0:
                            if(!connecting_new){                         
                                //when sending, first memset the packet to \0
                                memset(&packet, '\0', sizeof(packet));
                                //replace the text with random message
                                char message[] = "random text message";
                                //send to hardcoded test recipient
                                sprintf(packet, "%d %d %s %u %d %s", 1, 0, "jacek", this_ip, this_port, message);

                                printf("sending token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("0:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //we need to obtain prevnode nickname so we send the 3-type message
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 3, 0, nickname, this_ip, this_port, text);
                                printf(ANSI_COLOR_CYAN"Polling name token\n"ANSI_COLOR_RESET);
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("0:3:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 1:
                            //! remember to check retransimssion
                            if(strncmp(recp, nickname, 10) == 0){
                                //change type to 2 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 2, rtc, recp, sender_ip, sender_port, text);
                                printf("replying token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("1:Send failed");}
                                retransmission_timer = clock();
                            }else if(sender_port == this_port){
                                if(rtc < MAXRT){
                                    memset(&packet, '\0', sizeof(packet));
                                    rtc++;
                                    sprintf(packet, "%d %d %s %u %d %s", type, rtc, recp, sender_ip, sender_port, text);
                                    printf(ANSI_COLOR_YELLOW"token circled\n"ANSI_COLOR_RESET);
                                    if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("1:rt Send failed");}
                                    retransmission_timer = clock();
                                }else{
                                    memset(&packet, '\0', sizeof(packet));
                                    rtc++;
                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("clearing and sending token\n");
                                    if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("1:rtc0 Send failed");}
                                    retransmission_timer = clock();
                                }
                            }else{
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("1:rtc0 Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 2:
                            //for the sake of simplicity ill just compare the ports, since its localhost
                            //otherwise id need to check the machine's ip in network
                            if(sender_port == this_port){
                                //change type to 0 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                printf("accepting token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("2:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //change type to 0 and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", type, rtc, recp, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("2:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 3:
                            if(sender_port != this_port){
                                //replace the recipient with own data and send
                                memset(&packet, '\0', sizeof(packet));
                                sprintf(packet, "%d %d %s %u %d %s", type, rtc, nickname, sender_ip, sender_port, text);
                                printf("sending token\n");
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("3:Send failed");}
                                retransmission_timer = clock();
                            }else{
                                //change type to 4, fill properly and send
                                memset(&packet, '\0', sizeof(packet));
                                //change the sender ip and port to the one of new client and send
                                sprintf(packet, "%d %d %s %u %d %s", 4, 0, recp, tempaddr, tempport, text);
                                printf(ANSI_COLOR_CYAN"newnode token\n"ANSI_COLOR_RESET);
                                if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("3:Send failed");}
                                retransmission_timer = clock();
                            }
                            break;
                        case 4:
                            if(strncmp(recp, nickname, 10) == 0){
                                if(nextnode.sin_addr.s_addr != sender_ip || nextnode.sin_port != sender_port){
                                    memset(&packet, '\0', sizeof(packet));
                                    //change nextnode to the one supplied
                                    close(outsockfd);
                                    //extracted from packet
                                    nextnode.sin_family = AF_INET;
                                    nextnode.sin_addr.s_addr = sender_ip;
                                    nextnode.sin_port = htons(sender_port);
                                    outsockfd = socket(AF_INET , SOCK_STREAM , 0);if(outsockfd==-1){printf("%d: Could not create socket", __LINE__ );perror("socket");exit(EXIT_FAILURE);}
                                    printf(ANSI_COLOR_GREEN"connected new node\n"ANSI_COLOR_RESET);

                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("sending token\n");
                                    if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("4:Send failed");}
                                    retransmission_timer = clock();
                                }else{
                                    sprintf(packet, "%d %d %s %u %d %s", 0, rtc, recp, sender_ip, sender_port, text);
                                    printf("sending token\n");
                                    if(sendto(outsockfd, packet, sizeof(packet), 0, (struct sockaddr*)&nextnode, sizeof(nextnode)) < 0){puts("4:Send failed");}
                                    retransmission_timer = clock();
                                }
                            }
                            break;                            
                        default:
                            //malformed token, clear?
                            break;
                    }
                }
            }else{
                //the flag needs to be reset
                printf("no new message\n"); //debug
                errno = 0;
            }
        }

        //end udp
    }else{
        puts("unknown mode");
        cleanup();
    }    
}

/*
!note the sender of the packet always checks the signature to eliminate doubles
!and sets the timer, if no packet arrives in time, it retransmits it (only the sender, not intermediary)

how loop works tcp:
50/50 wait for token and new connections:
accept;
    new connection should either send service message with its addr or get it from tcp
    receiving client waits for empty token
    when token arrives, it sends special message, needing all the clients to supply their id (everyone replaces it with own)
    when it makes full circle, the sender knows who is before it, sends second message with that address and the new clients address
    when the receiving client gets the message, it changes his nextnode variable to supplied address

    !new client connected

recv;
check if contains my address (this instance's)
    if yes, continue parsing
        (should contain signature, dont touch it) !wait, then how will it change? will think about it later
    else, send to next node (saved in instance's config)

(possibly poll for keyboard input)
*/
