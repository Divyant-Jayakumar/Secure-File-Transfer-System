#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <stdint.h>
#include <unistd.h>
#include "server_functions.h"

typedef struct user_data {
    char user_ip[INET6_ADDRSTRLEN];
    char username[FIELD_SIZE_LIMIT];
} user_data;

typedef struct file_data{
    char filename[FIELD_SIZE_LIMIT];
    char password[FIELD_SIZE_LIMIT];    
    char file_owner[FIELD_SIZE_LIMIT];
    uint64_t filesize; 
} file_data;

user_data userdatabase[USERDATABASE_LIMIT];
file_data filedatabase[FILEDATABASE_LIMIT];

int main() {

    struct addrinfo *p, *res;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    int temp_status;
    int sockfd;
    int yes = 1;

    //sets up server socket with wildcard addres
    if ((temp_status = getaddrinfo(NULL, my_port, &hints, &res)) != 0) {
        fprintf(stderr, "Error in getaddrinfo(): %s\n", gai_strerror(temp_status));
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            perror("setsockopt");
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(sockfd);
            continue;
        }

        printf("Socket bound and ready!\n");
        char server_ip_presentation[INET6_ADDRSTRLEN];
        get_ip_presentation(p->ai_addr, server_ip_presentation);
        printf("Server ip: %s\n", server_ip_presentation);
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "Failed to bind to any address\n");
        return 1;
    }

    if (listen(sockfd, backlog) == -1) {
        perror("listen");
        return 1;
    }

    printf("Waiting for Client\n");

    int total_users = 0;
    int total_files = 0; 
    char message_to_client[250];
    int sizeof_message_to_client;
    char message_from_client[250];

    while(1){

        int existing_user_flag = 0;
        int clientfd;
        struct sockaddr_storage client_addr;
        char client_ip[INET6_ADDRSTRLEN];
        socklen_t client_addr_len = sizeof(client_addr);
        char client_username[FIELD_SIZE_LIMIT];
        int temp_sizeof;

        if ((clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
            perror("accept");
            return 1;
        }

        printf("Connected successfully to client\n");
        get_ip_presentation((struct sockaddr*)&client_addr, client_ip);
        printf("Client address: %s\n", client_ip);

        //check if user is existing user
        for(int i=0;i<total_users;i++){
            if(strcmp(client_ip,userdatabase[i].user_ip)==0){
                sprintf(message_to_client,"Welcome back %s!\n",userdatabase[i].username);
                sizeof_message_to_client = strlen(message_to_client);
                send_full(clientfd,message_to_client,&sizeof_message_to_client);  //error handle
                existing_user_flag = 1;
                break;
            }
        }

        if(!existing_user_flag){
            if(total_users == USERDATABASE_LIMIT) {
                
            }
            sprintf(message_to_client,"Hello new user! Please enter username: ");
            sizeof_message_to_client = strlen(message_to_client);
            send_full(clientfd,message_to_client,sizeof_message_to_client);
            receive_field_with_size(clientfd,client_username,temp_sizeof);
        }

        // Receive filename
        char filename[FIELD_SIZE_LIMIT];
        int filename_size;
        temp_status = receive_field_with_size(clientfd,filename,&filename_size); 
        if(temp_status == -1){fprintf(stderr,"Error while receiving filename\n");return 1;}
        else if (temp_status == -2) {fprintf(stderr,"Filename too long\n");return 1;} //make user try again

        uint64_t filesize;
        receive_uint64(clientfd, &filesize);
        printf("File size: %lu bytes\n", filesize);

        // Receive file
        if (receive_file(clientfd, filename, filesize) == -1) {
            fprintf(stderr, "Failed to receive file\n");
            close(clientfd);
            return 1;
        }

        printf("File received successfully!\n");
        close(clientfd);
   
    }

    close(sockfd);
    return 0;
}