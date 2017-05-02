#ifndef REMAI_CLIENT_H
#define REMAI_CLIENT_H

#include "reMAI-shared.h"

int sockfd;
WINDOW *confirmquitw;
WINDOW *inputw;
WINDOW *outputw;

void atquit(void);

void send(int, char *);

void receive(int *, char *);

void readthread(void);

void initncurses(void);

void manual(char *);

void initconnection(char *, char *);

void drawmenu(void);

void main(int, char **);

#endif
