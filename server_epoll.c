#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>

/* ?????????? ??????: ????????? ?????? ???????, ????? ??????? ????????
 ?? stdout, ? ????? ?????????? "Hello!".
 ??????????????????? ?????-?????? ?? ?????? epoll() */

#define DEFPORT 12346
#define SPARESLOTS 128
#define BUFSIZE 256

char server_reply[] = "Hello!";
int efd;

void accept_client(int);
void client_action(int); 

int main(int argc, char **argv) {
    int master_sock;
    struct sockaddr_in master_addr;
    unsigned short portnum = DEFPORT;
    struct epoll_event ev;

    printf("\nStarting Simple Server...\nUsage: %s [portnum]\n\n", argv[0]);
    //???????????? ????????? ???. ??????
    if (argc > 1) {
	if (sscanf(argv[1], "%hd", &portnum)!=1) portnum = DEFPORT;
    }
    //???????? ??????
    master_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_sock<0) {
	perror("socket() failed");
	exit(EXIT_FAILURE);
    }
    //??????? ????? ??????
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(portnum);
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(master_sock, (struct sockaddr*) &master_addr, sizeof(master_addr))) {
	perror("bind() failed");
	exit(EXIT_FAILURE);
    }
    //??????? ? ?????????????
    if (listen(master_sock, SOMAXCONN)) {
	perror("listen() failed");
	exit(EXIT_FAILURE);
    }
    printf("Listening port %d for incoming connections...\n", portnum);
    //?????????????? ?????????? epoll
    efd = epoll_create(SPARESLOTS);
    if (efd < 0) {
	perror("epoll() failed");
	exit(EXIT_FAILURE);
    }
    //??????????? ??? ?? ???????? ??????? ?? master_sock
    ev.events = EPOLLIN;
    ev.data.fd = master_sock;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, master_sock, &ev)) {
	perror("epoll() failed");
	exit(EXIT_FAILURE);
    }
    
    while (1) {
	int n;
	n = epoll_wait(efd, &ev, 1, -1);
	if (n < 0 && errno != EINTR) {
	    perror("poll() failed");
	    exit(EXIT_FAILURE);
	}
	if (n > 0) {
	    //??? ????? ???????
	    if (ev.data.fd == master_sock) accept_client(master_sock);
	    else client_action(ev.data.fd);
	}
    }
}

//????????? ?????? ???????
void accept_client(int msock) {
    int client_sock;
    struct sockaddr_in client_addr;
    struct epoll_event ev;
    socklen_t len;
    len = sizeof(client_addr);
    client_sock = accept(msock, (struct sockaddr*) &client_addr, &len);
    if (client_sock < 0) {
	perror("accept() failed");
	exit(EXIT_FAILURE);
    }
    printf("\nConnected client - %s:%u\n",
	inet_ntoa(client_addr.sin_addr),
	ntohs(client_addr.sin_port));
    ev.events = EPOLLIN;
    ev.data.fd = client_sock;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, client_sock, &ev)) {
	perror("epoll() failed");
	exit(EXIT_FAILURE);
    }
//    if (fcntl(client_sock, F_SETFL, O_NONBLOCK)) {
//	perror("failed to set non-blocking mode");
//    }
}

//???????? ???????
void client_action(int fd) {
    char buf[BUFSIZE];
    int len;
    printf("\nReceived client request:\n");
    do {
//	memset(buf, 0, BUFSIZE);
	len = read(fd, buf, BUFSIZE);
	if (len>0) write(STDOUT_FILENO, buf, len);
    } while (len == BUFSIZE);
    fflush(stdout);
    write(fd, server_reply, sizeof(server_reply));
    //close(fd);
}
