# Secure File Transfer System

This is a secure file transfer system in which clients can connect to a server to upload files that are both password protected and encrypted. Each time a client establishes a connection, a Diffie Hellman key exchange is performed to generate a shared secret key. Files are then encrypted using XOR encryption with this shared key before being transmitted, and decrypted upon receipt by the server. Clients (the original uploader or others) can later download these files provided they have the correct password. All communication between clients and the server is carried out over a TCP connection. We can transfer any type of file (text,images,pdfs,etc). 

## Features:
* key exchange using Diffie-Hellman Key Exchange (DHKE)  
* XOR encryption for all file transfers  
* User registration & recognition of existing users: new users asked to register by giving username and old users are welcomed back - found using client ip  
* File upload & download with password protection  
* File listing: view all files stored on the server  
* Overwrite protection: prompts before overwriting an existing file. Doesnt allow two users to have file with same name.  

## How it works:
When the server starts, it creates a TCP socket and binds it to the host’s IP address (Wildcard address). It is configured to listen on all available network interfaces, allowing it to accept connections from any network the host machine is connected to. The server also prints all available IP addresses along with their corresponding network interfaces for reference.  
A client connects to the server by providing the server’s IP address. It establishes a TCP connection by creating a socket and connecting it to the server’s address. Upon establishing a successful connection, a simple Diffie Hellman Key Exchange (DHKE) is performed using public parameters (prime = 5, base = 251). Both the client and server generate private keys randomly between 10 to 500 for each session, which are then used to compute a shared secret key. Files are encrypted by XOR encryption, where each byte of the file is XORed with the shared key.  

There are 4 commands: LIST, UPLOAD, DOWNLOAD, EXIT.  
LIST: Displays all files stored on the server, with the filename, owner, and size (in bytes)  
UPLOAD <filename> <password>: Uploads file to server with password. If the file owned by client is already present prompts for confirmation before updating it. If file with same name owned by someone else exists, asks client to change filename and try again   
DOWNLOAD <filename> <password>: we can download any file present on the server provided we know its passowrd  
EXIT: to end session  

All networking operations such as sending and receiving messages, along with many other functions are implemented in the network_functions library. The client and server communicate with each other by usind predefined status codes. Both the client and server print informative messages updating progress/status.  

## How to use: 

### Server:  
Download the Server folder  
Run "make -f Makefile_server"  
run the executable ./server  
Now the server is up and running, waiting for client.  

### Client:
Download the Client folder  
Run "make -f Makefile_client"  
run the executable ./client  
To connect to server we need to provide server IP or if we want to test internally enter 127.0.0.1  
If new user provide username  
Have files we want to upload inside this folder  
Enter required command  
NOTE: Filename must be given with extension. Ensure to use UPLOAD and DOWNLOAD with expected format. An error will be shown for invalid command or invalid format.  
To end session enter EXIT command  

## Constraints: 
Maximum total registered users: 25  
Maximum files that can be stores: 200  
Size limit for username, filename, password: 300  
File size limit: 0.5 GB  

