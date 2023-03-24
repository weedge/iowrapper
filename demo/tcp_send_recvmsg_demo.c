/*
    c recvmsg and sendmsg tcp server example
*/
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int listenfd, connfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUFFER_SIZE];
    struct msghdr msg = {0};
    struct iovec iov[1];
    int bytes_received, bytes_sent;

    // Create a TCP socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("Failed to create socket");
        exit(1);
    }

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8888);

    // Bind the socket to the server address
    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(listenfd, 10) < 0) {
        perror("Failed to listen for connections");
        exit(1);
    }

    printf("Server listening on port %d...\n", ntohs(server_addr.sin_port));

    // Accept incoming connections and receive/send data from/to clients
    while (1) {
        socklen_t client_len = sizeof(client_addr);
        connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd < 0) {
            perror("Failed to accept connection");
            continue;
        }

        printf("Accepted new connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Receive data from the client
        iov[0].iov_base = buffer;
        iov[0].iov_len = MAX_BUFFER_SIZE - 1;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        bytes_received = recvmsg(connfd, &msg, 0);
        if (bytes_received < 0) {
            perror("Failed to receive data from client");
            close(connfd);
            continue;
        }

        // Null-terminate the received data
        buffer[bytes_received] = '\0';

        printf("Received %d bytes from client %s:%d: %s\n",
               bytes_received, inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port), buffer);

        // Send a response to the client
        iov[0].iov_base = "Hello, client!";
        iov[0].iov_len = strlen("Hello, client!");
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        bytes_sent = sendmsg(connfd, &msg, 0);
        if (bytes_sent < 0) {
            perror("Failed to send data to client");
            close(connfd);
            continue;
        }

        printf("Sent %d bytes to client %s:%d\n",
               bytes_sent, inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        // Close the connection
        close(connfd);
    }

    // Close the listening socket
    close(listenfd);
}