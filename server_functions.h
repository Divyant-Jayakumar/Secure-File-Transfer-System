#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <endian.h>

#define FIELD_SIZE_LIMIT 100
#define USERDATABASE_LIMIT 25
#define FILEDATABASE_LIMIT 100

extern const char* my_port;
extern const int backlog;
extern const int PACKET_SIZE;

void get_ip_presentation(struct sockaddr* addr, char* client_ip);
int recv_full(int clientfd, char* message, int* len);
int receive_uint64(int clientfd, uint64_t* var);
int receive_int(int clientfd, int* var);
int receive_file(int clientfd, char* filename, uint64_t file_size);
int receive_field_with_size(int clientfd, char* field, int* field_size);

#endif