#include "reMAI-client.h"

int sockfd = -1;
WINDOW *confirmquitw = NULL;
WINDOW *inputw = NULL;
WINDOW *outputw = NULL;

void atquit(int arg)
{
	endwin();
	if (sockfd != -1)
		close(sockfd);
	if (confirmquitw != NULL)
		delwin(confirmquitw);
	if (inputw != NULL)
		delwin(inputw);
	if (outputw != NULL)
		delwin(outputw);
	exit(arg);
}

void initncurses(void)
{
	if (initscr() == NULL) {
		printf("Error during initializing ncurses.\n");
		exit(1);
	}
	confirmquitw = newwin(5, COLS/2, LINES/2 - 2, COLS/4);
	if (confirmquitw == NULL) {
		printw("Failed to initialize quit confirmation window.\n");
		refresh();
		getch();
		atquit(2);
	}
	wborder(confirmquitw, '#', '#', '#', '#', '#', '#', '#', '#');
	inputw = newwin(3, COLS, LINES-3, 0);
	if (inputw == NULL) {
		printw("Failed to initialize input window.\n");
		refresh();
		getch();
		atquit(3);
	} 
	outputw = newwin(LINES-3, COLS, 0, 0);
	if (outputw == NULL) {
		printw("Failed to initialize output window.\n");
		refresh();
		getch();
		atquit(4);
	}
	scrollok(inputw, TRUE);
	scrollok(outputw, TRUE);
	noecho();
	refresh();
}

void manual(char *name)
{
	printw("\nUsage: ");
	attron(A_BOLD);
	printw("%s server-ip-address nickname\n\n", name);
	attroff(A_BOLD);
	refresh();
	getch();
	atquit(5);
}

void initconnection(char *ip, char *nick)
{
	int len;
	struct sockaddr_in address;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printw("Failed to create client socket: %s.\n", strerror(errno));
		refresh();
		getch();
		atquit(6);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(ip);
	address.sin_port = htons(9000);
	len = sizeof(address);
	if (connect(sockfd, (struct sockaddr *)&address, len) == -1) {
		printw("Failed to connect to server: %s.\n", strerror(errno));
		refresh();
		getch();
		atquit(4);
	}
	send(CLIENT_NAME, nick);
	drawmenu();
	readthread();
}

void send(int code, char *mes)
{
	char buf[sizeof(int)];
	
	while (1) {
		write(sockfd, &code, sizeof(code));
		write(sockfd, mes, strlen(mes));
		memset(buf, '\0', sizeof(int));
		read(sockfd, buf, sizeof(int));
		if ((int)strtol(buf, NULL, 10) == AFFIRMATIVE)
			break;
	}
}

void receive(int *code, char *mes)
{
	memset(mes, '\0', BUFSIZE);
	read(sockfd, code, sizeof(code));
	read(sockfd, mes, BUFSIZE);
}

void readthread(void)
{
	
}

void drawmenu(void)
{
	char menuents[MAX_CHATS+1][BUFSIZE];
	int i, j, code;
	
	attron(A_BOLD);
	mvprintw(1, 3, "Choose a chat to join or an option:");
	attroff(A_BOLD);
	send(REQUEST_MENU, NULL);
	for (i = 0; i <= MAX_CHATS; i++) {
		receive(&code, menuents[i]);
		if (code == EXPLORE_MENU)
			break;
	}
	for (j = 0; j < i; j++)
		mvprintw(j+3, 3, "%s", menuents[j]);
	if (i != MAX_CHATS)
		mvprintw(j+4, 3, "Create new chat");
	else {
		attron(A_DIM);
		mvprintw(j+4, 3, "Cannot create new chat: max chat number reached");
		attroff(A_DIM);
	}
	mvprintw(j+5, 3, "Refresh chat list");
	mvprintw(j+7, 3, "Quit");
	refresh();
}

void main(int argc, char *argv[])
{
	initncurses();
	if (argc != 3)
		manual(argv[0]);
	initconnection(argv[1], argv[2]);
	
	atquit(0);
}
