#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "server_functions.h"

const char* my_port = "3942";
const int backlog = 10;
const int PACKET_SIZE = 16;

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

    if(recv_full(clientfd,field,field_size)==-1) return -1;
    field[*field_size]='\0';

    return 0; 
}

int message_client(int clientfd,char* message_to_client){

    int sizeof_message_to_client = strlen(message_to_client);
    return send_full(clientfd,message_to_client,&sizeof_message_to_client); 
}