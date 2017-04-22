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

#define BUFSIZE 512
/*#define CLIENT_MESSAGE	'0'
#define CLIENT_JOINED		'1'
#define CLIENT_LEAVED		'2'
#define CLIENT_CONNECTED	'3'
#define CLIENT_DISCONNECTED	'4'*/

int sockfd;

void *ReceiverThread(void *arg)
{
	char buf[BUFSIZE];
	
	while (1) {
		memset(buf, '\0', BUFSIZE);
		read(sockfd, buf, BUFSIZE);
		write(1, buf, strlen(buf));
	}
	pthread_exit(NULL);
}

void SenderThread(void)
{
	char buf[BUFSIZE];
//	char mes[BUFSIZE+1];
	
	while (1) {
		memset(buf, '\0', BUFSIZE);
//		memset(mes, '\0', BUFSIZE);
		read(0, buf, BUFSIZE);
//		mes[0] = CLIENT_MESSAGE;
//		strcat(mes, buf);
//		write(sockfd, mes, strlen(mes));
		write(sockfd, buf, strlen(buf));
	}
}

void main(void)
{
	int len;
	struct sockaddr_in address;
	int result;
	pthread_t sender, receiver;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(9734);
	len = sizeof(address);
	if (connect(sockfd, (struct sockaddr *)&address, len) == -1) {
		perror("client");
		exit(1);
	}
	
	
	pthread_create(&receiver, NULL, ReceiverThread, NULL);
	SenderThread();
	pthread_join(receiver, NULL);
	
	close(sockfd);
	exit(0);
}
