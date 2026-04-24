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
#include <sys/stat.h>
#include "network_functions.h"

const char* my_port = "3942";
const int backlog = 10;
const int PACKET_SIZE = 16;

void get_ip_presentation(struct sockaddr* addr, char* client_ip) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*) addr;
        inet_ntop(AF_INET, &ipv4_addr->sin_addr, client_ip, INET_ADDRSTRLEN); 
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
        if (n < 0) { fprintf(stderr, "Error occurred while receiving message\n"); break; }  
        else if (n == 0) { fprintf(stderr, "Connection closed!\n"); break; }
        total_recv += n;
    }
    *len = total_recv;
    return n<=0? -1 : 0;
}

int receive_uint64(int clientfd, uint64_t* var) {
    int len = sizeof(uint64_t);
    if (recv_full(clientfd, (char*)var, &len) == -1) {
        return -1;
    }

    *var = be64toh(*var);
    return 0; 
}

int receive_int(int clientfd, int* var) {
    int len = sizeof(int);
    if (recv_full(clientfd, (char*)var, &len) == -1) {
        return -1;
    }
    *var = ntohl(*var);
    return 0;
}

int receive_file(int clientfd, char* filename, uint64_t file_size, char shared_key) {
    FILE* fout = fopen(filename, "wb");
    if (fout == NULL) { fprintf(stderr, "Error in opening file\n"); return -1; }

    uint64_t total_recv = 0;
    int packet_size = PACKET_SIZE; 
    while (total_recv < file_size) {
        //change size of last packet 
        if(file_size - total_recv < PACKET_SIZE) packet_size = file_size - total_recv; 

        char packet[PACKET_SIZE];
        //receive packet
        if (recv_full(clientfd, packet, &packet_size) == -1) {
            fprintf(stderr, "Failed to receive packet\n");
            fclose(fout);
            return -1;
        }
        //encrypt data
        for(int i=0;i<packet_size;i++){
            packet[i] ^= shared_key;
        }
        //write packet into file
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

int receive_message (int clientfd, char* field){

    int field_size; 
    if(receive_int(clientfd,&field_size) == -1) return -1;  

    if(recv_full(clientfd,field,&field_size)==-1) return -1;
    field[field_size]='\0';

    return 0; 
}

int send_full(int sockfd, char* message, int* len) {
    int total_sent = 0;
    int n = 0;
    while (total_sent < *len) {
        n = send(sockfd, message + total_sent, *len - total_sent, 0);
        if (n < 0) { fprintf(stderr, "Error occurred while sending message\n"); break; }
        total_sent += n;
    }
    *len = total_sent;
    return n <= 0 ? -1 : 0;
}

uint64_t get_filesize(char* filename) {
    FILE* fin = fopen(filename, "rb");
    if (fin == NULL) { fprintf(stderr, "Error in opening file\n"); return 0; } 
    if (fseek(fin, 0L, SEEK_END) != 0) {
        fprintf(stderr, "fseek failed\n");
        fclose(fin);
        return 0;
    }

    uint64_t size = (uint64_t)ftell(fin);
    fclose(fin);              
    return size;
}

int send_file(int sockfd, char* filename, char shared_key) {
    uint64_t file_size = get_filesize(filename);
    if (file_size == 0) { fprintf(stderr, "File is empty or could not get size\n"); return -1; }

    FILE* fin = fopen(filename, "rb");
    if (fin == NULL) { fprintf(stderr, "Error in opening file\n"); return -1; }

    uint64_t total_sent = 0;
    int packet_size = PACKET_SIZE;

    while (total_sent < file_size) {
        //change size of last packet
        if(file_size - total_sent < PACKET_SIZE) packet_size = file_size - total_sent; 

        char packet[PACKET_SIZE];
        int bytes_read = 0;
        //read into packet
        while (bytes_read < packet_size) {
            size_t n = fread(packet + bytes_read, 1, packet_size - bytes_read, fin);
            if (n == 0 && ferror(fin)) {
                fprintf(stderr, "Failed to read packet\n");
                fclose(fin);
                return -1;
            }
            bytes_read += (int)n;
        }
        //encrypt data
        for(int i=0; i<packet_size;i++){
            packet[i] ^= shared_key;
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

int send_int(int sockfd, int value) {
    int value_net = htonl(value);
    int len = sizeof(int);
    return send_full(sockfd, (char*)&value_net, &len);
}

int send_uint64(int sockfd, uint64_t filesize){

    uint64_t filesize_net = htobe64(filesize);
    int len = sizeof(uint64_t);
    if (send_full(sockfd, (char*)&filesize_net, &len) == -1) {
        fprintf(stderr, "Failed to send file size\n");
        return -1;
    }

    return 0;
}

int send_message(int sockfd, char* field) {

    int field_size = strlen(field);
    if (field_size > MESSAGE_SIZE_LIMIT) return -2;
    //send the message size 
    if (send_int(sockfd, field_size) == -1) {
        fprintf(stderr, "Failed to send field size\n");
        return -1;
    }
    //send message
    if (send_full(sockfd, field, &field_size) == -1) {
        fprintf(stderr, "Failed to send field\n");
        return -1;
    }

    return 0;
}

int power (int base, int modulo, int exponent){
    base %= modulo;
    int res = base;
    for(int i=1;i<exponent;i++){
        res *= base;
        res %= modulo;  
    }
    return res;
}

unsigned char DHKE_server(int sockfd, int base, int prime, int private_key) {
    int public_key = power(base, prime, private_key);
    int client_public_key;
    receive_int(sockfd, &client_public_key);   // server receives first
    send_int(sockfd, public_key);            // then sends
    int shared_key = power(client_public_key, prime, private_key);
    return (unsigned char)shared_key;
}

unsigned char DHKE_client(int sockfd, int base, int prime, int private_key) {
    int public_key = power(base, prime, private_key);
    send_int(sockfd, public_key);            // client sends first
    int server_public_key;
    receive_int(sockfd, &server_public_key);   // then receives
    int shared_key = power(server_public_key, prime, private_key);
    return (unsigned char)shared_key;
}


