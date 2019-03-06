#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include "pop3.h"
#include "minini_12b/minIni.h"

#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))
#define SPARESLOTS 128

char server_reply[] = "Hello!";
int efd;

void accept_client(int);
void client_action(int);

int main() {
    server.port = pop3_server_port;

    struct epoll_event ev[SPARESLOTS];

    server.server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server.server_sock < 0)
    {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    server.server_addr.sin_family = AF_INET;
    server.server_addr.sin_port = htons(server.port);
    server.server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server.server_sock, (struct sockaddr*) &server.server_addr, sizeof(server.server_addr)))
    {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server.server_sock, SOMAXCONN))
    {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }
    printf("Server started. Listening port %d for incoming connections...\n", server.port);

    efd = epoll_create(SPARESLOTS);
    if (efd < 0)
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }

    ev[0].events = EPOLLIN;
    ev[0].data.ptr = &server;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, server.server_sock, &ev[0]))
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int n;
        n = epoll_wait(efd, ev, SPARESLOTS, -1);
        if (n < 0 && errno != EINTR)
        {
            perror("poll() failed");
            exit(EXIT_FAILURE);
        }
        printf("Epoll events: %d\n", n);
        if (n > 0)
        {
            for(int i = 0; i < n; i++)
            {
                struct server srv = *(struct server *)ev[i].data.ptr;
                struct client cli = *(struct client *)ev[i].data.ptr;
                if (srv.server_sock == server.server_sock) {
                    printf("Epoll triggered: incoming connection\n");
                    accept_client(server.server_sock);
                } else {
                    printf("Epoll triggered: data from client\n");
                    client_action(cli.client_sock);
                }
            }
        }
    }
}

void accept_client(int socket) {
    struct client client;
    struct epoll_event evs;
    socklen_t len;
    len = sizeof(client.client_addr);
    client.client_sock = accept(socket, (struct sockaddr*) &client.client_addr, &len);
    if (client.client_sock < 0)
    {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected client - %s:%u\n", inet_ntoa(client.client_addr.sin_addr), ntohs(client.client_addr.sin_port));
    evs.events = EPOLLIN;
    evs.data.ptr = &client;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, client.client_sock, &evs))
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }
}

void client_action(int socket) {
    struct epoll_event evs;
    size_t len = 0;
    printf("Received client request:\n");
    ioctl(socket, FIONREAD, &len);
    char buf[len];
    read(socket, buf, len);
    write(STDOUT_FILENO, buf, len);
    fflush(stdout);
    write(socket, buf, len);
    struct client client;
    client.client_sock = socket;
    evs.events = EPOLLIN;
    evs.data.ptr = &client;
    if (epoll_ctl(efd, EPOLL_CTL_DEL, socket, NULL))
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }
    if (epoll_ctl(efd, EPOLL_CTL_ADD, socket, &evs))
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }
}