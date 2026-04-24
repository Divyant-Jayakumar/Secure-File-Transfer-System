#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "network_functions.h"

typedef struct user_data {
    char user_ip[INET6_ADDRSTRLEN];
    char username[MESSAGE_SIZE_LIMIT];
} user_data;

typedef struct file_data {
    char filename[MESSAGE_SIZE_LIMIT];
    char password[MESSAGE_SIZE_LIMIT];
    char file_owner[MESSAGE_SIZE_LIMIT];
    char owner_ip[INET6_ADDRSTRLEN];
    uint64_t filesize;
} file_data;

user_data userdatabase[USERDATABASE_LIMIT];
file_data filedatabase[FILEDATABASE_LIMIT];
//for DHKE
const int base = 5;
const int prime = 251;

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

    //create socket and bind to host IP (Wildcard address)
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

    printf("Waiting for client...\n");

    int total_users = 0;
    int total_files = 0;
    char message_to_client[MESSAGE_SIZE_LIMIT];
    char message_from_client[MESSAGE_SIZE_LIMIT];

    //accept and process client one by one
    while (1) {

        
        int clientfd;
        struct sockaddr_storage client_addr;
        char client_ip[INET6_ADDRSTRLEN];
        socklen_t client_addr_len = sizeof(client_addr);
        char client_username[MESSAGE_SIZE_LIMIT];
        int existing_user_flag = 0; 
        
        int command_to_client;
        int command_from_client;

        //accept client
        if ((clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
            perror("accept");
            continue;
        }

        printf("Connected successfully to client\n");
        get_ip_presentation((struct sockaddr*)&client_addr, client_ip);
        printf("Client address: %s\n", client_ip);

        //diffie hellman key exchange
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        int private_key = (rand() % 490) + 10; //generate a random private key between 10 and 500
        unsigned char shared_key = DHKE_server(clientfd, base, prime, private_key);

        // check if existing user
        for (int i = 0; i < total_users; i++) {
            if (strcmp(client_ip, userdatabase[i].user_ip) == 0) {
                strcpy(client_username, userdatabase[i].username);
                command_to_client = 5; //letting client know its an existing user
                send_int(clientfd, command_to_client);
                sprintf(message_to_client, "Welcome back %s!", client_username);
                send_message(clientfd, message_to_client);
                existing_user_flag = 1;
                break;
            }
        }

        // new user
        if (!existing_user_flag) {
            if (total_users == USERDATABASE_LIMIT) {
                command_to_client = 7; //letting client know server reached max existing user limit
                send_int(clientfd, command_to_client);
                close(clientfd);
                continue;
            }

            command_to_client = 6; //letting client know we can process a new user
            send_int(clientfd, command_to_client);
            //receive necessary details from client and create a user entry
            receive_message(clientfd, client_username);
            strcpy(userdatabase[total_users].user_ip, client_ip);
            strcpy(userdatabase[total_users].username, client_username);

            sprintf(message_to_client, "Welcome %s!", client_username);
            send_message(clientfd, message_to_client);
            total_users++;
        }

        // command processing loop
        while (1) {

            if (receive_message(clientfd, message_from_client) == -1) {
                printf("Client disconnected unexpectedly\n");
                break;
            }

            // LIST - lists details of all existing files
            if (strcmp(message_from_client, "LIST") == 0) {
                send_int(clientfd, total_files);
                for (int i = 0; i < total_files; i++) {
                    send_message(clientfd, filedatabase[i].filename);
                    send_message(clientfd, filedatabase[i].file_owner);
                    send_uint64(clientfd, filedatabase[i].filesize);
                }
            }

            // UPLOAD - receives file with password and stores it
            else if (strcmp(message_from_client, "UPLOAD") == 0) {

                if (total_files == FILEDATABASE_LIMIT) {
                    command_to_client = 208; //letting client know server reached max storage capacity
                    send_int(clientfd, command_to_client);
                    continue;
                }

                command_to_client = 207;//letting client know file can be uploaded
                send_int(clientfd, command_to_client);
                //receive metadata of file
                char filename[MESSAGE_SIZE_LIMIT];
                char password[MESSAGE_SIZE_LIMIT];
                uint64_t filesize;

                receive_message(clientfd, filename);
                receive_message(clientfd, password);
                receive_uint64(clientfd, &filesize);

                // check if file already exists for this user
                int flag_file_present = 0;
                int existing_file_index = -1;
                for (int i = 0; i < total_files; i++) {
                    if (strcmp(filename, filedatabase[i].filename) == 0 &&
                        strcmp(client_ip, filedatabase[i].owner_ip) == 0) {
                        flag_file_present = 1;
                        existing_file_index = i;
                        break;
                    }
                }

                if (flag_file_present) {
                    command_to_client = 201; //letting client know file already exists
                    send_int(clientfd, command_to_client);
                    receive_int(clientfd, &command_from_client);
                    if (command_from_client == 204) continue; //client cancels upload
                } 
                else {
                    command_to_client = 202; //letting client know file doesnt already exist
                    send_int(clientfd, command_to_client);
                }

                if (receive_file(clientfd, filename, filesize, shared_key) == -1) {
                    fprintf(stderr, "Failed to receive file\n");
                    command_to_client = 206; //letting client know failed to receive file
                    send_int(clientfd, command_to_client);
                    continue;
                }

                command_to_client = 205; //letting client know file uploaded sucessfully
                send_int(clientfd, command_to_client);

                // update existing entry or add new one
                int index = flag_file_present ? existing_file_index : total_files;
                strcpy(filedatabase[index].filename, filename);
                strcpy(filedatabase[index].password, password);
                strcpy(filedatabase[index].file_owner, client_username);
                strcpy(filedatabase[index].owner_ip, client_ip);
                filedatabase[index].filesize = filesize;
                if (!flag_file_present) total_files++;
            }

            // DOWNLOAD
            else if (strcmp(message_from_client, "DOWNLOAD") == 0) {
                //receive matadata of file
                char filename[MESSAGE_SIZE_LIMIT];
                char password[MESSAGE_SIZE_LIMIT];
                receive_message(clientfd, filename);
                receive_message(clientfd, password);
                //checking if file present in server
                int flag_file_found = 0;
                int file_index = -1;
                for (int i = 0; i < total_files; i++) {
                    if (strcmp(filename, filedatabase[i].filename) == 0) {
                        flag_file_found = 1;
                        file_index = i;
                        break;
                    }
                }

                if (!flag_file_found) {
                    command_to_client = 302; //letting client know file not present
                    send_int(clientfd, command_to_client);
                    continue;
                }

                command_to_client = 301; //letting client know file is present
                send_int(clientfd, command_to_client);
                //checking password
                if (strcmp(password, filedatabase[file_index].password) != 0) {
                    command_to_client = 304;//letting client know password incorrect
                    send_int(clientfd, command_to_client);
                    continue;
                }

                command_to_client = 303; //letting client know password correct 
                send_int(clientfd, command_to_client);

                send_uint64(clientfd, filedatabase[file_index].filesize);
                // sending the file to client 
                if (send_file(clientfd, filename, shared_key) == -1) {
                    command_to_client = 306;//letting client know error while sending file 
                    send_int(clientfd, command_to_client);
                    continue;
                }

                command_to_client = 305; //letting client know file sent without error
                send_int(clientfd, command_to_client);
            }

            // EXIT
            else if (strcmp(message_from_client, "EXIT") == 0) {
                printf("Client %s disconnected\n", client_username);
                close(clientfd);
                break;
            }

            else {
                printf("Unknown command received: %s\n", message_from_client);
            }
        }
    }

    close(sockfd);
    return 0;
}