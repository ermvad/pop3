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

int efd;

void accept_client(int);
void client_action(struct client *);
void client_action_command(struct client *, char *, size_t);
void client_close_connection(struct client *, char *);
void print_command(char *);

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
                if (srv.server_sock == server.server_sock)
                {
                    printf("Epoll triggered: incoming connection\n");
                    accept_client(server.server_sock);
                }
                else
                {
                    struct client *cli = (struct client *)ev[i].data.ptr;
                    printf("Epoll triggered: data from client\n");
                    client_action(cli);
                }
            }
        }
    }
}

void accept_client(int socket) {
    struct client *client = (struct client*) malloc(sizeof(struct client));
    struct epoll_event evs;
    socklen_t len;
    len = sizeof(client->client_addr);
    client->client_sock = accept(socket, (struct sockaddr*) &(client->client_addr), &len);
    if (client->client_sock < 0)
    {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }
    client->connection_status = 1;
    client->pop3_session_status = 0;
    printf("Connected client - %s:%u\n", inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));
    evs.events = EPOLLIN;
    evs.data.ptr = client;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, client->client_sock, &evs))
    {
        perror("epoll() failed");
        exit(EXIT_FAILURE);
    }
    write(client->client_sock, "+OK POP3 server ready\r\n", 23);
}

void client_action(struct client *client)
{
    size_t len = 0;
    ioctl(client->client_sock, FIONREAD, &len);
    if(!len)
    {
        client_close_connection(client, "no available bytes to read");
    }
    else
    {
        printf("Received client request: bytes to read: %d\n", len);
        char buf[len-1];
        read(client->client_sock, buf, len);
        client_action_command(client, buf, len);
    }
}

void client_close_connection(struct client *client, char *reason)
{
    close(client->client_sock);
    client->connection_status = 0;
    client->pop3_session_status = 0;
    free(client);
    printf("Client closed connection: %s\n", reason);
    fflush(stdout);
}

void print_command(char *cmd)
{
    printf("Command from client: %s\n", cmd);
}

void client_action_command(struct client *client, char *cmd, size_t len)
{
    //write(STDOUT_FILENO, cmd, len);
    //fflush(stdout);
    //if(len != write(client->client_sock, cmd, len))
    //    client_close_connection(client);
    char cmd_line[len-1], cmd_line_tmp[len-1];
    memmove(cmd_line, cmd, len);
    cmd_line[len-1] = '\0';
    printf("Received data from client: %s\n", cmd_line);
    strcpy(cmd_line_tmp,cmd_line);
    char *saveptr;
    char *pch = strtok_r (cmd_line_tmp, " ", &saveptr);
    int cmd_args = 0;
    char cmd_token[pop3_max_arg][len-1];
    while (pch != NULL)
    {
        if(cmd_args > pop3_max_arg)
        {
            client_close_connection(client, "too many arguments passed");
            break;
        }
        else
        {
            strcpy(cmd_token[cmd_args], pch);
            cmd_args++;
            pch = strtok_r(NULL, " \n\r", &saveptr);
        }
    }
    if(strcmp(cmd_token[0], "CAPA") == 0)
    {
        print_command(cmd_token[0]);
        write(client->client_sock, "+OK List of supported mechanisms follows:\r\nLOGIN\r\nPLAIN\r\n.\r\n", 60);
    }
    else if(strcmp(cmd_token[0], "AUTH'") == 0)
    {
        print_command(cmd_token[0]);
    }
    else if(strcmp(cmd_token[0], "USER") == 0)
    {
        print_command(cmd_token[0]);
        if((client->connection_status == 1) && (client->pop3_session_status == 0))
        {
            char key[40];
            ini_gets("pop3", "accounts", "AAA", key, 40, postboxes_config_file);
            printf("%s\n", key);
            print_command(cmd_token[0]);
        }
    }
    else if(strcmp(cmd_token[0], "PASS") == 0)
    {
        print_command(cmd_token[0]);
    }
    //for(int i=0; i<cmd_args; i++)
    //{
    //   printf("%s\n", cmd_token[i]);
    //}
}