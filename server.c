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
#include <inttypes.h>

struct file_data {
    char filename[100];
    char client_name[100];
    char password[100];
    uint64_t filesize; 
};

const char* my_port = "3942";
const int backlog = 10;
const int PACKET_SIZE = 16;

void get_ip_presentation(struct sockaddr* addr,char * client_ip){

    if(addr->sa_family == AF_INET){
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in* ) addr;
        inet_ntop(AF_INET,&ipv4_addr->sin_addr,client_ip,INET_ADDRSTRLEN);
        return; 
    }
    
    struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6* ) addr;
    inet_ntop(AF_INET6,&ipv6_addr->sin6_addr,client_ip,INET6_ADDRSTRLEN);
    
}

void receive_uint64(int clientfd, uint64_t* var){  // change to receive fully
    recv(clientfd, var, sizeof(uint64_t), 0);
    *var = be64toh(*var);
}

void receive_int(int clientfd,int* var){

    recv(clientfd,var,sizeof(int),0);
    *var = ntohl(*var);
}

int recv_full(int sockfd, char* message, int* len){  
    int total_recv = 0; 
    int n;
    while(total_recv<*len){
        n = recv(sockfd,message+total_recv,*len - total_recv,0);
        if(n<0) {fprintf(stderr,"Error occured while receiving message\n"); break;}
        else if(n==0) {fprintf(stderr,"Connection closed!\n"); break;}
        total_recv += n; 
    }

    *len = total_recv;
    return n<=0?-1:0;  
}

int receive_file(int clientfd,char* filename,uint64_t file_size){

    FILE* fin = fopen(filename,"wb"); 
    if (fin == NULL) {fprintf(stderr,"Error in opening file\n"); return -1; }
    
    uint64_t total_recv = 0; 
    int packet_size = PACKET_SIZE; 

    while(total_recv<file_size){

        if(file_size - total_recv < PACKET_SIZE) packet_size = file_size - total_recv;  
        char packet[packet_size]; 
        int n;
        int bytes_written = 0;

        if(recv_full(clientfd,packet,&packet_size)==-1) {fprintf(stderr,"Failed to send packet"); return -1;}
        total_recv += packet_size; 

        while(bytes_written<packet_size){
            n = fwrite(packet+bytes_written,1,packet_size - bytes_written,fin);
            if(n==0) {fprintf(stderr,"Failed to load packet\n"); return -1; }
            bytes_written += n; 
        }

    }

    fclose(fin); 
    return 0;
}

// int handle_recv_error(int n){
//     if(n<0) {fprintf(stderr,"Error occured while receiving message\n"); return -1;}
//     else if(n==0) {fprintf(stderr,"Connection closed!\n"); return -1;}
//     return 0;
// }

int get_field (int sockfd, int* field_size_host, char* field){
    int *field_size = field_size_host;
    int n;

    n = recv(sockfd,field_size,sizeof(int),0);  // should be recv_int & handle error
    *field_size_host = ntohl(*field_size);

    if(recv_full(sockfd,field,field_size_host)==-1) return -1;
    field[*field_size_host]='\0';

    return 0; 
}

int main(){

    struct addrinfo *p,*res; 
    struct addrinfo hints; 

    memset(&hints,0,sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC; 

    int status; 
    int sockfd; 
    int yes=1;
    if((status = getaddrinfo(NULL,my_port,&hints,&res))!=0){
        printf("Error occured in getaddrinfo()\n");
        return 1;
    }

    for(p = res; p!=NULL; p = p->ai_next){
        
        if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1){
            continue;
        }

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

        if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1){
            continue;
        }

        printf("Socket binded and ready!\n");
        char server_ip_presentation[INET6_ADDRSTRLEN];
        get_ip_presentation(p->ai_addr,server_ip_presentation);
        printf("Server ip: %s\n",server_ip_presentation);
        break;
    }

    listen(sockfd,backlog);

    int clientfd;
    struct sockaddr_storage client_addr; 
    char client_ip[INET6_ADDRSTRLEN];
    socklen_t client_addr_len = sizeof(client_addr);
    if((clientfd = accept(sockfd,(struct sockaddr*)&client_addr,&client_addr_len))==-1) printf("Acccept Failed\n");
    else{
        printf("Connected Sucessfully to client\n");
        get_ip_presentation((struct sockaddr*) &client_addr,client_ip);
        printf("Client address: %s\n",client_ip);
    } 
    
    char filename[100];
    int filename_size; 
    int filename_size_host; 
    uint64_t filesize;

    // recv(clientfd,&filename_size,sizeof(int),0);
    // filename_size_host = ntohl(filename_size);

    // recv(clientfd,filename,filename_size_host,0);
    // filename[filename_size_host]='\0';
    // printf("File name: %s\n",filename);

    get_field(clientfd,&filename_size_host,filename);
    printf("Filename: %s\n",filename);
    
    recv(clientfd, &filesize, sizeof(uint64_t),0);
    uint64_t  filesize_host; 
    filesize_host = be64toh(filesize);
    printf("%" PRIu64 "\n", filesize_host);

    receive_file(clientfd,filename,filesize_host);
    
    printf("Execution finished sucessfully\n"); // print only if sucessful
}
