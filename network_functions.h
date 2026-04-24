#ifndef NETWORK_FUNCTIONS_H
#define NETWORK_FUNCTIONS_H

#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <endian.h>

#define MESSAGE_SIZE_LIMIT 300
#define USERDATABASE_LIMIT 25
#define FILEDATABASE_LIMIT 100

extern const char* my_port;
extern const int backlog;
extern const int PACKET_SIZE;

void get_ip_presentation(struct sockaddr* addr, char* client_ip);
int recv_full(int sockfd, char* message, int* len);
int receive_uint64(int sockfd, uint64_t* var);
int receive_int(int sockfd, int* var);
int receive_file(int sockfd, char* filename, uint64_t file_size);
int receive_message(int sockfd, char* field);
int send_full(int sockfd, char* message, int* len);
uint64_t get_filesize(char* filename);
int send_file(int sockfd, char* filename);
int send_message(int sockfd, char* field);
int send_int(int sockfd, int value);
int send_uint64(int sockfd, uint64_t filesize);

#endif
