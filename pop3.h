#ifndef _POP3_H_
#define _POP3_H_

#include <netinet/in.h>

#define pop3_server_port 12345
#define postboxes_config_file "postboxes.ini"
#define postboxes_folder_path "postboxes"
#define account_config_file "account.ini"
#define max_token_length 64
#define pop3_max_arg 5

struct client
{
    int client_sock;
    int connection_status; // 1 - connected, 0 disconnected
    int pop3_session_status; // 0 - unauthorized, 1 - authorization, 2 - transaction, 3 - update
    int busy;
    char *name;
    struct sockaddr_in client_addr;
};
struct server
{
    int port;
    int server_sock;
    struct sockaddr_in server_addr;
}server;
#endif
