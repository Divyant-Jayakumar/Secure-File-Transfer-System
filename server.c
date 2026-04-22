#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <endian.h>

const char* my_port = "3942";
const int backlog = 10;
const int PACKET_SIZE = 16;
#define FIELD_SIZE_LIMIT 1000

void get_ip_presentation(struct sockaddr* addr, char* client_ip) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*) addr;
        inet_ntop(AF_INET, &ipv4_addr->sin_addr, client_ip, INET_ADDRSTRLEN); // include error handling? 
        return;
    }
    struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*) addr;
    inet_ntop(AF_INET6, &ipv6_addr->sin6_addr, client_ip, INET6_ADDRSTRLEN);
}

int recv_full(int clientfd, char* message, int* len) {
    int total_recv = 0;
    int n = 0;
    while (total_recv < *len) {
        n = recv(clientfd, message + total_recv, *len - total_recv, 0);
        if (n < 0) { fprintf(stderr, "Error occurred while receiving message\n"); break; }  // better error handling messages?
        else if (n == 0) { fprintf(stderr, "Connection closed!\n"); break; }
        total_recv += n;
    }
    *len = total_recv;
    return n<=0? -1 : 0;
}

int receive_uint64(int clientfd, uint64_t* var) {
    int len = sizeof(uint64_t);
    if (recv_full(clientfd, (char*)var, &len) == -1) {
        //fprintf(stderr, "Failed to receive uint64\n");   // change error message? 
        return -1;
    }

    *var = be64toh(*var);
    return 0; 
}

int receive_int(int clientfd, int* var) {
    int len = sizeof(int);
    if (recv_full(clientfd, (char*)var, &len) == -1) {
        //fprintf(stderr, "Failed to receive int\n");   // change error message? 
        return -1;
    }
    *var = ntohl(*var);
    return 0;
}

int receive_file(int clientfd, char* filename, uint64_t file_size) {
    FILE* fout = fopen(filename, "wb");
    if (fout == NULL) { fprintf(stderr, "Error in opening file\n"); return -1; }

    uint64_t total_recv = 0;
    int packet_size = PACKET_SIZE; // no need packet size once we include encryption 
    while (total_recv < file_size) {

        if(file_size - total_recv < PACKET_SIZE) packet_size = file_size - total_recv; 

        char packet[PACKET_SIZE];

        if (recv_full(clientfd, packet, &packet_size) == -1) {
            fprintf(stderr, "Failed to receive packet\n");
            fclose(fout);
            return -1;
        }

        int bytes_written = 0;

        while (bytes_written < packet_size) {
            size_t n = fwrite(packet + bytes_written, 1, packet_size - bytes_written, fout);
            if (n == 0 && ferror(fout)) {
                fprintf(stderr, "Failed to write packet\n");
                fclose(fout);
                return -1;
            }
            bytes_written += (int)n;
        }

        total_recv += packet_size;
    }

    fclose(fout);
    return 0;
} 

int receive_field_with_size (int clientfd, char* field, int* field_size){

    if(receive_int(clientfd,field_size) == -1) return -1;  
    if(*field_size > FIELD_SIZE_LIMIT-1) return -2;   // may not be needed, checked in client side 

    if(recv_full(clientfd,field,field_size)==-1) return -1;
    field[*field_size]='\0';

    return 0; 
}

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

    int clientfd;
    struct sockaddr_storage client_addr;
    char client_ip[INET6_ADDRSTRLEN];
    socklen_t client_addr_len = sizeof(client_addr);

    if ((clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
        perror("accept");
        return 1;
    }

    printf("Connected successfully to client\n");
    get_ip_presentation((struct sockaddr*)&client_addr, client_ip);
    printf("Client address: %s\n", client_ip);

    // Receive filename
    char filename[FIELD_SIZE_LIMIT];
    int filename_size;
    temp_status = receive_field_with_size(clientfd,filename,&filename_size); 
    if(temp_status == -1){fprintf(stderr,"Error while receiving filename\n");return 1;}
    else if (temp_status == -2) {fprintf(stderr,"Filename too long\n");return 1;} //make user try again
    // receive_int(clientfd, &filename_size);

    // if (recv_full(clientfd, filename, &filename_size) == -1) {
    //     fprintf(stderr, "Failed to receive filename\n");
    //     close(clientfd);
    //     return 1;
    // }
    // filename[filename_size] = '\0';
    // printf("File name: %s\n", filename);

    // Receive file size
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
    close(sockfd);
    return 0;
}