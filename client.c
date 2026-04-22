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
#include<sys/stat.h>
#include <endian.h>
#include <inttypes.h>

const char* my_port = "3942";
const int PACKET_SIZE = 16; 

int send_full(int sockfd, char* message, int* len){
    int total_sent = 0; 
    int n;
    while(total_sent<*len){
        n = send(sockfd,message+total_sent,*len - total_sent,0);
        if(n<0) {fprintf(stderr,"Error occured while sending message\n"); break; }
        total_sent += n; 
    }

    *len = total_sent;
    return n==-1?-1:0;  
}

uint64_t get_filesize(char* filename){
    FILE* fin = fopen(filename,"rb"); 
    if (fin == NULL) {fprintf(stderr,"Error in opening file\n"); return -1; }
    fseek(fin,0L,SEEK_END);
    uint64_t size = ftell(fin);
    fclose(fin); 
    
    return size; 
}

int send_file(int sockfd, char* filename){
    long file_size = get_filesize(filename);
    FILE* fin = fopen(filename,"rb"); 
    if (fin == NULL) {fprintf(stderr,"Error in opening file\n"); return -1; }
    
    long total_sent = 0; 
    int packet_size = PACKET_SIZE; 

    while(total_sent<file_size){

        if(file_size - total_sent < PACKET_SIZE) packet_size = file_size - total_sent;  
        char packet[packet_size]; 
        int n;
        int bytes_read = 0;

        while(bytes_read<packet_size){
            n = fread(packet+bytes_read,1,packet_size - bytes_read,fin);
            if(n==0) {fprintf(stderr,"Failed to load packet\n"); return -1; }
            bytes_read += n; 
        }

        if(send_full(sockfd,packet,&packet_size)==-1) {fprintf(stderr,"Failed to send packet"); return -1;}
        total_sent += packet_size; 
    }

    fclose(fin);
    return 0;

}

int main(){
    struct addrinfo *p,*res; 
    struct addrinfo hints; 

    memset(&hints,0,sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC; 

    int status; 
    int sockfd; 
    if((status = getaddrinfo("127.0.0.1",my_port,&hints,&res))!=0){
        printf("Error occured in getaddrinfo()\n");
        return 1;
    }

    int flag = 0;
    for(p = res; p!=NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1){
            continue;
        }
        if(connect(sockfd,(struct sockaddr*)p->ai_addr,p->ai_addrlen)==-1){
            continue;
        }

        printf("Connected to server sucessfully!\n");
        flag = 1;
        break;
    }

    if(flag==0) printf("Wasnt able to make connection\n"); // not working

    
    char filename[100] = "file1.txt";
    int filename_len = strlen(filename);
    int filename_size_net = htonl(filename_len);
    send(sockfd, &filename_size_net, sizeof(int), 0);
    send(sockfd, filename, filename_len, 0);

    uint64_t filesize = get_filesize(filename);
    uint64_t filesize_network = htobe64(filesize);
    printf("%" PRIu64 "\n", filesize);
    send(sockfd,&filesize_network,sizeof(filesize_network),0);

    send_file(sockfd, filename);

    //char password[100]="Divi";
     

    printf("Execution finished sucessfully\n");
}