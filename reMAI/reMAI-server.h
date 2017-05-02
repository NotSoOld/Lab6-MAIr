#ifndef REMAI_SERVER_H
#define REMAI_SERVER_H

#include "reMAI-shared.h"
#include <netdb.h>
#include <sys/ioctl.h>
#include <time.h>

#define MAX_CHATS 32
#define MAX_USERS_PER_CHAT 32
#define MAX_USERS_IN_LOBBY 32

struct clientinfo {
	int fd;
	int addrlen;
	int col;
	char name[80];
	struct sockaddr_in addr;
};

struct chatinfo {
	int curclients;
	char title[80];
	struct clientinfo clients[MAX_USERS_PER_CHAT];
	fd_set clientfds;
	int maxfd;
	pthread_mutex_t mx_chat;
};

struct chatinfo chats[MAX_CHATS];
struct chatinfo lobby;
int numofchats;
int server_sockfd;
pthread_mutex_t mx_numofchats;
pthread_mutex_t mx_allchats;

#endif
