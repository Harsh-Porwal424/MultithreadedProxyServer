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

int main(int argc, char *argv[]){
    int client_socketID, client_len;
    struct sockaddr server_addr, client_addr;
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
 

}



