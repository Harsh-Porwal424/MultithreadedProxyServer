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
#define MAX_BYTES 4096

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

int connectRemoteServer(char* host_addr, int port_number){

    int remoteSocketId = socket(AF_INET, SOCK_STREAM, 0);

    if(remoteSocketId < 0){
        printf("Error to create a socket\n");
        return -1;
    }
    struct hostent* host = gethostbyname(host_addr);
    if(host == NULL){
        fprintf(stderr, "Error to get host by name\n");
        return -1; 
    }

    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);

    bcopy((char *)&host->h_addr, (char *)&server_addr.sin_addr.s_addr, host->h_length);

    if(connect(remoteSocketId, (struct sockaddr *)&server_addr, (size_t)sizeof(server_addr) < 0)){
        fprintf(stderr, "Error to connect to the server\n");
        return -1;
    }
    return remoteSocketId;

}

int handle_request(int clientSocketId, ParsedRequest *request, char* tempReq){

    char* buf = (char *)malloc(sizeof(char)*MAX_BYTES);
    strcpy(buf,"GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    if(ParsedHeader_set(request, "Connection", "close") < 0){
        printf("Failed to set Connection: close\n");
        printf("Set header key is failed\n");
    }

    if(ParsedHeader_get(request, "Host") == NULL){
        if(ParsedHeader_set(request, "Host", request->host) < 0){
            printf("Failed to set Host\n");
            printf("Set Host header key is not working\n");
        }
    }

    if(ParsedRequest_unparse_headers(request, buf+len, (size_t)MAX_BYTES - len) < 0){
        printf("Failed to unparse headers\n");
    }

    int server_port = 80;
    if(request->port != NULL){
        server_port = atoi(request->port);
    }

    int remoteSockerId = connectRemoteServer(request->host, server_port);

    if(remoteSockerId < 0){
        printf("Failed to connect to remote server\n");
        close(clientSocketId);
        close(remoteSockerId);
        return -1;
    }

    int bytes_send = send(remoteSockerId, buf, strlen(buf),0);
    bzero(buf, MAX_BYTES);

    bytes_send = recv(remoteSockerId, buf, MAX_BYTES-1, 0);

    char *temp_buffer = (char*)malloc(MAX_BYTES*sizeof(char));
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while(bytes_send > 0){
        //client thread ko bhej rahe
        bytes_send = send(clientSocketId, buf, bytes_send, 0);
        //data temp buffer me store kar rahe, to add in cache later
        for(int i = 0; i < bytes_send/sizeof(char); i++){
            temp_buffer[temp_buffer_index++] = buf[i];
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char *)realloc(temp_buffer, temp_buffer_size);
        if(bytes_send < 0){
            perror("Error in sending data to client\n");
            break;
        }
        bzero(buf, MAX_BYTES);
        //again website server se recieve kar rahe
        bytes_send = recv(remoteSockerId, buf, MAX_BYTES-1, 0);
    }

    return -1;

}

void *thread_fn_handle_client(void *socketNew){

    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore, &p);
    printf("Current Semaphore Value is: %d\n", p);
    int *t = (int*)socketNew;
    int socket = *t;
    int bytes_send_client, len;

    char *buffer = (char*)calloc(MAX_BYTES, sizeof(char));

    bzero(buffer, MAX_BYTES);
    
    //recv is used to receive data from a socket. it returns the number of bytes received.
    //What is use of buffer? It's used to store the data received from the client.
    //How it is buffer is storing data from the client ? 
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0);
    
    //Taking request from client & using recv till end of http request
    while(bytes_send_client > 0){
        len = strlen(buffer);
        if(strstr(buffer, "\r\n\r\n")!= NULL){
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        }
        else{
            break;
        }
    }
    
    //what is bytes_send_client? It's used to store the number of bytes received from the client.
    //Matlab buffer me client ka data store ho raha hai.

    char *tempReq = (char*)malloc(strlen(buffer)*sizeof(char)+1);
    for(int i = 0; i < strlen(buffer) && buffer[i]!= '\0'; i++){
        tempReq[i] = buffer[i];
    }
    //strcpy(tempReq, buffer);

    //Extracting the URL from the request
    //Temp pointer of my LRU cache.
    struct cache_element* temp = find(tempReq);

    if(temp != NULL){
        int size = temp->len/sizeof(char);
        int pos = 0;
        char* response[MAX_BYTES];
        while(pos < size){
            bzero(response, MAX_BYTES);
            for(int i = 0; i < MAX_BYTES; i++){
                response[i] = temp->data[i++];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data Retrived from the cache\n");
        printf("%s\n\n", response);
    }
    else if(bytes_send_client > 0){
        len = strlen(buffer);
        //Abb hamare pas cache me element nahi hai, to abb ham parsed_request library use kar ke http request ko parse karenge
        ParsedRequest *request = ParsedRequest_create();

        if(ParsedRequest_parse(request, buffer, len) < 0){
            printf("Error parsing request\n");
        }
        else{
            //If parsing successful, then we will handle response & create a new cache element and add it to the cache
            bzero(buffer, MAX_BYTES);
            if(!strcmp(request->method, "GET")){
                //Abb hamare request me GET request hai, to abb ham server me data ko request karenge
                //And server me data ko response karenge
                if(request->host && request->path && checkHTTPVersion(request->version)==1){
                    bytes_send_client = handle_request(socket,request, tempReq);
                    if(bytes_send_client == -1){
                        sendErrorMessage(socket, 500, "Internal Server Error");
                        printf("Error in handling request\n");
                    }
                }
                else{
                    sendErrorMessage(socket, 400, "Bad Request");
                    printf("Invalid request\n");
                }
            }
            else{
                printf("This Code does not suppory any method apart from GET\n");
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(bytes_send_client == 0){
        printf("Client Disconnected\n");
    }
    //closing the socket and freeing memory
    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, &p);
    printf("Current Semaphore Post Value is: %d\n", p);
    free(tempReq);
    return NULL;

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



