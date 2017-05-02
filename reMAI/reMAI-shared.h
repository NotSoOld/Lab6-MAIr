#ifndef REMAI_SHARED_H
#define REMAI_SHARED_H

#define _REENTRANT

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ncurses.h>
#include <pthread.h>

#define BUFSIZE 512

enum mescode {
	AFFIRMATIVE,
	CLIENT_NAME,
	REQUEST_MENU,
	SENDING_MENU,
	EXPLORE_MENU,
	REQUEST_CREATE_CHAT,
	NEW_CHAT_NAME,
	CANCEL_NEW_CHAT,
	REQUEST_JOIN_CHAT,
	REQUEST_QUIT_CHAT,
	QUITED_CHAT,
	TEXT_MESSAGE,
	REQUEST_EXIT,
	EXITED
};

struct message {
	int code;
	char contents[BUFSIZE];
};

#endif
