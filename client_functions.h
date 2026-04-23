#ifndef CLIENT_FUNCTIONS_H
#define CLIENT_FUNCTIONS_H

#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <endian.h>

#define FIELD_SIZE_LIMIT 1000

extern const char* my_port;
extern const int PACKET_SIZE;


int send_full(int sockfd, char* message, int* len);

uint64_t get_filesize(char* filename);

int send_file(int sockfd, char* filename);

int send_field_with_size(int sockfd, char* field);

#endif