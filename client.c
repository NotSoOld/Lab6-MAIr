#define _REENTRANT

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <ncurses.h>
#include <errno.h>

#define BUFSIZE 512

int sockfd;
WINDOW *inputW;
WINDOW *outputW;

void AtExit(int code)
{
	wgetch(outputW);
	close(sockfd);
	delwin(inputW);
	delwin(outputW);
	endwin();
	exit(code);
}

void AtInterruption(int sig)
{
	write(sockfd, "!killed", 7);
	AtExit(sig);
}

void *ReceiverThread(void *arg)
{
	char buf[BUFSIZE];
	
	while (1) {
		memset(buf, '\0', BUFSIZE);
		if (read(sockfd, buf, BUFSIZE) <= 0) {
			wprintw(outputW, "Failed to read from server.\nIf server"
							 " was shut down, ignore this message.\n");
			break;
		}
		/* This is signal from server 
		 * (while attempting to quit in lobby or interrupt itself). 
		 */
		if (strcmp(buf, "!term") == 0) {
			AtInterruption(7);
			break;
		}
		wprintw(outputW, "%s\n", buf);
		wrefresh(outputW);
		wrefresh(inputW);
	}

	pthread_exit(NULL);
}

void SenderThread(char *name)
{
	char buf[BUFSIZE];
	
	if (write(sockfd, name, strlen(name)) == -1) {
		wprintw(outputW, "Failed to send nickname to server: %s.\n", strerror(errno));
		wrefresh(outputW);
	}
	while (1) {
		wmove(inputW, 1, 0);
		wclrtobot(inputW);
		wborder(inputW, ' ', ' ', 0, ' ', '-', '-', ' ', ' ');
		memset(buf, '\0', BUFSIZE);
		wattron(inputW, A_BOLD | A_STANDOUT);
		wprintw(inputW, "-ME: ");
		wattroff(inputW, A_BOLD | A_STANDOUT);
		wrefresh(inputW);
		wgetnstr(inputW, buf, BUFSIZE);
		wrefresh(inputW);
		if (write(sockfd, buf, strlen(buf)) == -1) {
			wprintw(outputW, "Failed to write to server: %s.\n", strerror(errno));
			wrefresh(outputW);
			continue;
		}
		wattron(outputW, A_BOLD | A_STANDOUT);
		wprintw(outputW, "-ME: ");
		wattroff(outputW, A_BOLD | A_STANDOUT);
		wprintw(outputW, "%s\n", buf);
		wrefresh(outputW);
	}
}

void main(int argc, char *argv[])
{
	int len;
	struct sockaddr_in address;
	pthread_t receiver;
	
	signal(SIGINT, AtInterruption);
	if (initscr() == NULL) {
		printf("Error during initializing ncurses.\n");
		AtExit(1);
	}
	inputW = newwin(4, COLS, LINES-4, 0);
	if (inputW == NULL) {
		printw("Failed to initialize input window.\n");
		refresh();
		AtExit(2);
	} 
	outputW = newwin(LINES-4, COLS, 0, 0);
	if (outputW == NULL) {
		printw("Failed to initialize output window.\n");
		refresh();
		AtExit(2);
	}
	scrollok(inputW, TRUE);
	scrollok(outputW, TRUE);
	wborder(inputW, ' ', ' ', 0, ' ', '-', '-', ' ', ' ');
	wrefresh(inputW);
	if (argc != 3) {
		wprintw(outputW, "\nUsage: ");
		wattron(outputW, A_BOLD);
		wprintw(outputW, "%s server-ip-address nickname\n\n", argv[0]);
		wattroff(outputW, A_BOLD);
		wrefresh(outputW);
		AtExit(2);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		wprintw(outputW, "Failed to create client socket: %s.\n", strerror(errno));
		wrefresh(outputW);
		AtExit(3);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(argv[1]);
	address.sin_port = htons(9000);
	len = sizeof(address);
	if (connect(sockfd, (struct sockaddr *)&address, len) == -1) {
		wprintw(outputW, "Failed to connect to server: %s.\n", strerror(errno));
		wrefresh(outputW);
		AtExit(4);
	}
	if (pthread_create(&receiver, NULL, ReceiverThread, NULL) != 0) {
		wprintw(outputW, "Failed to create receiver thread: %s.\n", strerror(errno));
		wrefresh(outputW);
		AtInterruption(5);
	}
	SenderThread(argv[2]);
	if (pthread_join(receiver, NULL) != 0) {
		wprintw(outputW, "Failed to join receiver thread: %s.\n", strerror(errno));
		wrefresh(outputW);
		AtInterruption(6);
	}
	AtExit(0);
}
