//read server code first to know the meaning of each code sent by server
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
//for DHKE
const int base = 5;
const int prime = 251;

int main() {

    struct addrinfo *p, *res;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    int temp_status;
    int sockfd;
    int command_from_server;
    int command_to_server;
    char message_to_server[MESSAGE_SIZE_LIMIT];
    char message_from_server[MESSAGE_SIZE_LIMIT];

    //get server IP
    char server_ip[INET6_ADDRSTRLEN];
    printf("Enter server IP: ");
    scanf("%s", server_ip);
    //connect to server
    if ((temp_status = getaddrinfo(server_ip, my_port, &hints, &res)) != 0) {
        fprintf(stderr, "Error in getaddrinfo(): %s\n", gai_strerror(temp_status));
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        if (connect(sockfd, (struct sockaddr*)p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }
        printf("Connected to server successfully!\n");
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "Could not connect to server\n");
        return 1;
    }

    //diffie hellman key exchange
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    int private_key = (rand() % 490) + 10;
    unsigned char shared_key = DHKE_client(sockfd, base, prime, private_key);

    // handle registration/login based on code received from server
    receive_int(sockfd, &command_from_server);

    if (command_from_server == 7) {
        printf("Server reached max user limit! Please try again later\n");
        close(sockfd);
        return 0;
    } 
    else if (command_from_server == 6) {
        printf("Hello new user! Please enter username: ");
        scanf("%s", message_to_server);
        send_message(sockfd, message_to_server);
        receive_message(sockfd, message_from_server);
        printf("%s\n", message_from_server);
    } 
    else if (command_from_server == 5) {
        receive_message(sockfd, message_from_server);
        printf("%s\n", message_from_server);
    }

    printf("\nCOMMANDS:\n");
    printf("LIST\n");
    printf("UPLOAD <filename> <password>\n");
    printf("DOWNLOAD <filename> <password>\n");
    printf("EXIT\n\n");

    // Consume leftover newline from scanf before entering fgets loop
    char discard[MESSAGE_SIZE_LIMIT];
    fgets(discard, sizeof(discard), stdin);
    //command processing loop
    while (1) {

        char input_line[MESSAGE_SIZE_LIMIT*3];
        char command[MESSAGE_SIZE_LIMIT];

        printf("Enter command: ");
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) break;

        // strip trailing newline
        input_line[strcspn(input_line, "\n")] = '\0';

        // parse just the command first
        if (sscanf(input_line, "%s", command) != 1) continue;

        // LIST
        if (strcmp(command, "LIST") == 0) {
            send_message(sockfd, command);

            int total_files;
            receive_int(sockfd, &total_files);

            if (total_files == 0) {
                printf("No files stored on server\n");
                continue;
            }

            char filename[MESSAGE_SIZE_LIMIT];
            char file_owner[MESSAGE_SIZE_LIMIT];
            uint64_t filesize;

            printf("\n%-5s %-30s %-20s %s\n", "No.", "Filename", "Owner", "Size (bytes)");
            printf("----------------------------------------------------------------------\n");
            for (int i = 0; i < total_files; i++) {
                receive_message(sockfd, filename);
                receive_message(sockfd, file_owner);
                receive_uint64(sockfd, &filesize);
                printf("%-5d %-30s %-20s %lu\n", i + 1, filename, file_owner, filesize);
            }
            printf("\n");
        }

        // UPLOAD
        else if (strcmp(command, "UPLOAD") == 0) {

            char filename[MESSAGE_SIZE_LIMIT];
            char password[MESSAGE_SIZE_LIMIT];
            //check if input format correct
            if (sscanf(input_line, "%s %s %s",command, filename, password) != 3) {
                printf("Please enter in format: UPLOAD <filename> <password>\n");
                continue;
            }

            // check file exists locally before contacting server
            uint64_t filesize = get_filesize(filename);
            if (filesize == 0) {
                printf("File not found or empty: %s\n", filename);
                continue;
            }

            send_message(sockfd, command);
            receive_int(sockfd, &command_from_server);

            if (command_from_server == 208) {
                printf("Server storage capacity full!\n");
                continue;
            }

            // 207 - proceed
            send_message(sockfd, filename);
            send_message(sockfd, password);
            send_uint64(sockfd, filesize);

            receive_int(sockfd, &command_from_server);
            //file already present - we can still send or cancel upload
            if (command_from_server == 201) {
                char reply_line[10];
                char reply;
                printf("File already present on server. Overwrite it? (y/n): ");
                fgets(reply_line, sizeof(reply_line), stdin);
                reply = reply_line[0];

                if (reply == 'y') {
                    command_to_server = 203;
                    send_int(sockfd, command_to_server);
                } else {
                    command_to_server = 204;
                    send_int(sockfd, command_to_server);
                    continue;
                }
            }
            else if(command_from_server == 209){ // file exists by different owner 
                printf("File with same name owned by another user! Please change filename and try again\n");
                continue;
            }

            // 202 - it is new file, proceed

            //send file
            if (send_file(sockfd, filename, shared_key) == -1) {
                printf("Error sending file\n");
                continue;
            }
            //upload status received
            receive_int(sockfd, &command_from_server);
            if (command_from_server == 205) {
                printf("File uploaded successfully!\n");
            } 
            else if (command_from_server == 206) {
                printf("Error uploading file! Please try again\n");
            }
        }

        // DOWNLOAD
        else if (strcmp(command, "DOWNLOAD") == 0) {

            char filename[MESSAGE_SIZE_LIMIT];
            char password[MESSAGE_SIZE_LIMIT];
            //check if input in correct format
            if (sscanf(input_line, "%s %s %s", command, filename, password) != 3) {
                printf("Please enter in format: DOWNLOAD <filename> <password>\n");
                continue;
            }
            
            send_message(sockfd, command);
            send_message(sockfd, filename);
            send_message(sockfd, password);

            receive_int(sockfd, &command_from_server);
            if (command_from_server == 302) {
                printf("File not found on server!\n");
                continue;
            }

            // 301 - file present
            receive_int(sockfd, &command_from_server);
            if (command_from_server == 304) {
                printf("Incorrect password!\n");
                continue;
            }

            // 303 - password correct
            uint64_t filesize;
            receive_uint64(sockfd, &filesize);

            if (receive_file(sockfd, filename, filesize, shared_key) == -1) {
                printf("Error receiving file! Please try again\n");
                receive_int(sockfd, &command_from_server);
                continue;
            }
            //receive file transmission status
            receive_int(sockfd, &command_from_server);
            if (command_from_server == 305) {
                printf("File downloaded successfully!\n");
            } 
            else if (command_from_server == 306) {
                printf("Error downloading file! Please try again\n");
            }
        }

        // EXIT
        else if (strcmp(command, "EXIT") == 0) {
            send_message(sockfd, command);
            printf("Exited. Thanks for using Secure-File-Transfer-System\n");
            close(sockfd);
            break;
        }

        else {
            printf("Invalid command!\n");
        }
    }

    return 0;
}
