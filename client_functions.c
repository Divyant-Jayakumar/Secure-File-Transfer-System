#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "client_functions.h"

const char* my_port = "3942";
const int PACKET_SIZE = 16;

int send_full(int sockfd, char* message, int* len) {
    int total_sent = 0;
    int n = 0;
    while (total_sent < *len) {
        n = send(sockfd, message + total_sent, *len - total_sent, 0);
        if (n < 0) { fprintf(stderr, "Error occurred while sending message\n"); break; }
        total_sent += n;
    }
    *len = total_sent;
    return n == -1 ? -1 : 0;
}

uint64_t get_filesize(char* filename) {
    FILE* fin = fopen(filename, "rb");
    if (fin == NULL) { fprintf(stderr, "Error in opening file\n"); return 0; } // Caution 0 means error
    if (fseek(fin, 0L, SEEK_END) != 0) {
        fprintf(stderr, "fseek failed\n");
        fclose(fin);
        return 0;
    }

    return (uint64_t)ftell(fin);
    fclose(fin);
}

int send_file(int sockfd, char* filename) {
    uint64_t file_size = get_filesize(filename);
    if (file_size == 0) { fprintf(stderr, "File is empty or could not get size\n"); return -1; }

    FILE* fin = fopen(filename, "rb");
    if (fin == NULL) { fprintf(stderr, "Error in opening file\n"); return -1; }

    uint64_t total_sent = 0;
    int packet_size = PACKET_SIZE;

    while (total_sent < file_size) {

        if(file_size - total_sent < PACKET_SIZE) packet_size = file_size - total_sent; 

        char packet[PACKET_SIZE];
        int bytes_read = 0;

        while (bytes_read < packet_size) {
            size_t n = fread(packet + bytes_read, 1, packet_size - bytes_read, fin);
            if (n == 0 && ferror(fin)) {
                fprintf(stderr, "Failed to read packet\n");
                fclose(fin);
                return -1;
            }
            bytes_read += (int)n;
        }

        if (send_full(sockfd, packet, &packet_size) == -1) {
            fprintf(stderr, "Failed to send packet\n");
            fclose(fin);
            return -1;
        }

        total_sent += packet_size;
    }

    fclose(fin);
    return 0;
}

int send_field_with_size(int sockfd, char* field){

    if(strlen(field)>FIELD_SIZE_LIMIT-1) return -2;
    
    //field length sent first
    int field_size = strlen(field);
    int field_size_net = htonl(field_size); 
    int len = sizeof(int);
    if (send_full(sockfd, (char*)&field_size_net, &len) == -1) {
        fprintf(stderr, "Failed to send field size\n");
        return -1;
    }
    // sending field 
    if (send_full(sockfd, field, &field_size) == -1) {
        fprintf(stderr, "Failed to send field\n");
        return -1;
    }

    return 0;
}