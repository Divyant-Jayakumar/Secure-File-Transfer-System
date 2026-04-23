#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <stdint.h>
#include <unistd.h>
#include "client_functions.h"

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