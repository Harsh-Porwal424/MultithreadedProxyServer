#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_CLIENTS 10

typedef struct cache_element cache_element;

//LRU CACHE ARCH.
struct cache_element{
    char* data;
    int len;
    char* url;
    time_t lru_time_track;
    cache_element* next;
};

//LRU Cache Functions to USe
cache_element* find(char* url);
int add_cache_element(char* data, int size, char* url);
void remove_cache_element();

int port_number = 8080;


// Function to create a socket and bind it to a local address
//Main Socket in which incoming connections will be accepted
int proxy_socketId;

pthread_t tid[MAX_CLIENTS];
//semaphore is used to limit the number of simultaneous connections of sockets initiated by clients to the proxy server
sem_t semaphore;
//lock is used to prevent race conditions in cache operations during parallel execution
pthread_mutex_t lock;

cache_element* head;
int cache_size;

void *thread_fn_handle_client(void *socketNew){
    
}

int main(int argc, char *argv[]){
    int client_socketID, client_len;
    //sockaddr is a generic structure for socket addresses. It contains address family, port number, and IP address.
    //server_addr and client_addr are used to store the address of the server and client respectively
    //sockaddr_in socket address internet style (IPv4)
    struct sockaddr_in server_addr, client_addr;

    //initializing semaphore & lock
    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if(argv == 2){
        port_number = atoi(argv[1]);
    }
    else{
        printf("Too few arguments. Please provide port number.\n");
        exit(1);
    }

    printf("Starting proxy server at port: %d\n", port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0) {
        perror("ERROR in creating a socket");
        exit(1);
    }
    int reuse = 1;
    //Configuting the main proxy_socketId to reuse the same address and port
    //setsockopt function is used to set options on sockets. It consists of setting the SO_REUSEADDR option.
    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }

    //cleaning the server_addr structure
    bzero((char *) &server_addr, sizeof(server_addr));
    //Initializing the server_addr structure
    server_addr.sin_family = AF_INET;
    //htons is used, so that the networks is able to understand the port number in network byte order
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //binding the main proxy_socketId to the server_addr
    if(bind(proxy_socketId, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        perror("ERROR on binding, Port is not available\n");
        exit(1);
    }
    printf("Proxy server is listening... Binded on port: %d\n", port_number);
    //listening for incoming connections

    int listen_status = listen(proxy_socketId, MAX_CLIENTS);

    if(listen_status < 0){
        perror("ERROR on listening\n");
        exit(1);
    }

    //definnig the iterator.
    int i = 0;
    //Connected_socketId is used to store the socket IDs of the connected clients. 
    int Connected_socketId[MAX_CLIENTS];

    while(1){
        bzero((char *) &client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);

        //accept is used to accept incoming connections. It blocks until a client connects to the server.
        client_socketID = accept(proxy_socketId, (struct sockaddr *) &client_addr, (socklen_t *)&client_len);
        if(client_socketID < 0){
            printf("Not Able to accept connection\n");
            exit(1);
        }
        else{
            Connected_socketId[i] = client_socketID;
        }

        //copying the client_addr pointer to client_pt for easier use
        struct sockaddr_in * client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        
        //in_ntop converts an Internet address in network byte order to a printable string in IPv4
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Connection established with client IP: %s, Port: %d\n", str, ntohs(client_pt->sin_port));
        
        //yaha se ham multiple threads me sockets initiate karenge. jo parallel execution se karenge.
        pthread_create(&tid[i], NULL, thread_fn_handle_client, (void *)&Connected_socketId[i]);
        i++;
    }
    close(proxy_socketId);
    return 0;

 

}



