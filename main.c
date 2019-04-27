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
char **accounts;
int accounts_num = 0;

void accept_client(int);
void client_action(struct client *);
void client_action_command(struct client *, char *, size_t);
void client_close_connection(struct client *, char *);
void print_command(char *);
char** tokenizer(char *, char *, int, int, int *);
void load_accounts();

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

    load_accounts();

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

void load_accounts()
{
    char key[100];
    ini_gets("pop3", "accounts", "NULL", key, sizeof(key), postboxes_config_file);
    accounts = tokenizer(key, ",", 128, 101, &accounts_num);
    for(int i = 0; i < accounts_num; i++)
    {
        printf("Loaded account: %s\n", accounts[i]);
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

char** tokenizer(char *cmd, char *token, int max_tokens, int len, int *tokens_num)
{
    char cmd_tmp[len-1];
    strcpy(cmd_tmp,cmd);
    char *saveptr;
    char *pch = strtok_r (cmd_tmp, token, &saveptr);
    int cmd_args = 0;
    char **cmd_token = (char**) malloc(max_tokens * sizeof(char*));
    cmd_token[0] = (char*) malloc(max_token_length * sizeof(char));
    while (pch != NULL)
    {
        strcpy(cmd_token[cmd_args], pch);
        cmd_args++;
        cmd_token[cmd_args] = (char*) malloc(max_token_length * sizeof(char));
        pch = strtok_r(NULL, token, &saveptr);
    }
    free(cmd_token[cmd_args]);
    *tokens_num = cmd_args;
    return cmd_token;
}

void client_action_command(struct client *client, char *cmd, size_t len)
{
    //write(STDOUT_FILENO, cmd, len);
    //fflush(stdout);
    //if(len != write(client->client_sock, cmd, len))
    //    client_close_connection(client);
    char cmd_line[len-1];//, cmd_line_tmp[len-1];
    memmove(cmd_line, cmd, len);
    cmd_line[len-1] = '\0';
    printf("Received data from client: %s\n", cmd_line);
    /*strcpy(cmd_line_tmp,cmd_line);
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
    }*/
    int tokens_num = 0;
    char **cmd_token = tokenizer(cmd_line, " \r\n", 6, len, &tokens_num);
    if(strcmp(cmd_token[0], "USER") == 0)
    {
        if((client->connection_status == 1) && (client->pop3_session_status == 0))
        {
            for(int i = 0; i < accounts_num; i++)
            {
                if(strcmp(accounts[i], cmd_token[1]) == 0)
                {
                    client->pop3_session_status = 1;
                    write(client->client_sock, "+OK Name is valid mailbox\r\n", 27);
                    strcpy(client->name, cmd_token[1]);
                }
            }
        }
    }
    else if(strcmp(cmd_token[0], "PASS") == 0)
    {
        char key[100];
        char path[100];
        sprintf(path, "%s/%s/%s", postboxes_folder_path, client->name, account_config_file);
        ini_gets("account", "password", "NULL", key, sizeof(key), path);
        if(strcmp(key, cmd_token[1]) == 0)
        {
            client->pop3_session_status = 2;
            write(client->client_sock, "+OK Mailbox locked and ready\r\n", 30);
            ///
            char keys[100];
            char paths[100];
            sprintf(paths, "%s/%s/%s", postboxes_folder_path, client->name, account_config_file);
            ini_gets("mail", "mails", "NULL", keys, sizeof(keys), paths);
            int letters_num = 0;
            tokenizer(keys, ",", 64, 8, &letters_num);
            client->total_letters = letters_num;
        }
    }
    else if(strcmp(cmd_token[0], "LIST") == 0)
    {
        if(client->pop3_session_status == 2)
        {
            char key[100];
            char path[100];
            sprintf(path, "%s/%s/%s", postboxes_folder_path, client->name, account_config_file);
            ini_gets("mail", "mails", "NULL", key, sizeof(key), path);
            int letters_num = 0;
            char **letters = tokenizer(key, ",", 64, 8, &letters_num);
            //
            char response[1000];
            sprintf(response, "+OK scan listing follows\r\n");
            for(int i = 0; i < client->total_letters; i++)
            {
                sprintf(response, "%s%s %d\r\n", response, letters[i], (i*100)+100);
            }
            if(client->total_letters)
            {
                sprintf(response, "%s.\r\n", response);
            }
            printf("resp: %s", response);
            write(client->client_sock, response, strlen(response));
            //
            for(int i = 0; i < client->total_letters; i++)
            {
                free(letters[i]);
            }
            free(letters);
        }
    }
    else if(strcmp(cmd_token[0], "STAT") == 0)
    {
        if(client->pop3_session_status == 2)
        {
            int val = 0;
            for(int i = 0; i < client->total_letters; i++)
            {
                val += (i*100)+100;
            }
            char response[100];
            sprintf(response, "+OK %d %d\r\n", client->total_letters, val);
            printf("res: %s", response);
            write(client->client_sock, response, strlen(response));
        }

    }
    else if(strcmp(cmd_token[0], "RETR") == 0)
    {
        if(client->pop3_session_status == 2)
        {
            FILE *f;
            char path[50];
            sprintf(path, "%s/%s/%s", postboxes_folder_path, client->name, cmd_token[1]);
            if((f = fopen(path, "r")) == NULL)
            {
                write(client->client_sock, "-ERR\r\n", 6);
            }
            else
            {
                char buffer[128], response[2048];
                sprintf(response, "+OK, Message follows:\r\n");
                //while (fscanf (f, "%s", buffer) != EOF)
                while (fgets (buffer, 128, f) != NULL)
                {
                    sprintf(buffer, "%s", buffer);
                    sprintf(response, "%s%s", response, buffer);
                }
                sprintf(response, "%s.\r\n", response);
                printf("%s", response);
                write(client->client_sock, response, strlen(response));
                fclose(f);
            }
        }
    }
    else if(strcmp(cmd_token[0], "NOOP") == 0)
    {
        write(client->client_sock, "+OK\r\n", 5);
    }
    else if(strcmp(cmd_token[0], "QUIT") == 0)
    {
        write(client->client_sock, "+OK\r\n", 5);
        client_close_connection(client, "QUIT command");
    }
    else
    {
        write(client->client_sock, "-ERR\r\n", 6);
    }
    for(int i = 0; i < tokens_num; i++)
    {
        free(cmd_token[i]);
    }
    free(cmd_token);
}