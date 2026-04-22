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
#include <sys/stat.h>
#include <endian.h>

const char* my_port = "3942";
const int PACKET_SIZE = 16;
#define FIELD_SIZE_LIMIT 1000

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

int main() {
    struct addrinfo *p, *res;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    int temp_status;
    int sockfd;

    if ((temp_status = getaddrinfo("127.0.0.1", my_port, &hints, &res)) != 0) {
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
        fprintf(stderr, "Failed to bind to any address\n");
        return -1;
    }


    char filename[FIELD_SIZE_LIMIT] = "file1.txt";

    // sent filename with size
    temp_status = send_field_with_size(sockfd,filename);
    if(temp_status == -1) {fprintf(stderr,"Error in sending filename\n"); return 1;}
    else if (temp_status == -2) {printf("File name too long!\n");}

    // Send file size
    uint64_t filesize = get_filesize(filename);
    if (filesize == 0) {
        fprintf(stderr, "Failed to get file size\n");
        return 1;
    }
    uint64_t filesize_net = htobe64(filesize);
    int len = sizeof(uint64_t);
    if (send_full(sockfd, (char*)&filesize_net, &len) == -1) {
        fprintf(stderr, "Failed to send file size\n");
        return 1;
    }

    // Send file
    if (send_file(sockfd, filename) == -1) {
        fprintf(stderr, "Failed to send file\n");
        close(sockfd);
        return 1;
    }

    printf("File sent successfully!\n");
    close(sockfd);
    return 0;
}