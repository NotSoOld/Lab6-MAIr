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

#define BUFSIZE 512

int sockfd;

void AtInterruption(int sig)
{
	char *out;
	
	out = "!exit\n";
	write(sockfd, out, strlen(out));
	close(sockfd);
	exit(sig);
}

void *ReceiverThread(void *arg)
{
	char buf[BUFSIZE];
	
	while (1) {
		memset(buf, '\0', BUFSIZE);
		if (read(sockfd, buf, BUFSIZE) <= 0) {
			perror("Failed to read from server");
			//break;
			//continue;
		}
		// This is signal from server (while attempting to quit in lobby).
		if (strcmp(buf, "!term") == 0) {
			// In case we were in chat:
			AtInterruption(6);
			break;
		}
		write(1, "\r", 1);
		write(1, buf, strlen(buf));
		write(1, "\033[1;7m-ME:\033[0m ", 15);
	}
	pthread_exit(NULL);
}

void SenderThread(void)
{
	char buf[BUFSIZE];
	
	while (1) {
		memset(buf, '\0', BUFSIZE);
		write(1, "\033[1;7m-ME:\033[0m ", 15);
		read(0, buf, BUFSIZE);
		if (write(sockfd, buf, strlen(buf)) == -1) {
			perror("Failed to write to server");
			//break;
			//continue;
		}
	}
}

void main(void)
{
	int len;
	struct sockaddr_in address;
	int result;
	pthread_t sender, receiver;
	
	signal(SIGINT, AtInterruption);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Failed to create client socket");
		exit(1);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(9734);
	len = sizeof(address);
	if (connect(sockfd, (struct sockaddr *)&address, len) == -1) {
		perror("Failed to connect to server");
		exit(3);
	}
	
	if (pthread_create(&receiver, NULL, ReceiverThread, NULL) != 0) {
		perror("Failed to create receiver thread");
		AtInterruption(4);
	}
	SenderThread();
	if (pthread_join(receiver, NULL) != 0) {
		perror("Failed to join receiver thread");
		AtInterruption(5);
	}
	
	close(sockfd);
	exit(0);
}
