#define _REENTRANT

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFSIZE 512

int clientsfd[BUFSIZE];
int clientslen[BUFSIZE];
int curclients;
struct sockaddr_in clients_addr[BUFSIZE];

void EraseClient(int n)
{
	int i, j;
	
	curclients--;
	for (i = n; i < curclients; i++) {
		clientsfd[i] = clientsfd[i+1];
		clientslen[i] = clientslen[i+1];
		clients_addr[i] = clients_addr[i+1];
	}
}

void *Thread_Lobby(void *arg)
{
	char buf[BUFSIZE];
	int i, j;
	
	while (1) {
		for (i = 0; i < curclients; i++) {
			memset(buf, '\0', BUFSIZE);
			read(clientsfd[i], buf, BUFSIZE);
			if (strcmp(buf, "!exit") == 0) {
				close(clientsfd[i]);
				EraseClient(i);
				continue;
			}
			for (j = 0; j < curclients; j++)
				if (i != j)
					write(clientsfd[j], buf, strlen(buf));
		}
	}
	
	pthread_exit(NULL);
}


void main(void)
{
	int server_sockfd;
	int server_len;
	struct sockaddr_in server_address;
	char buf[BUFSIZE];
	
	memset(clientsfd, 0, BUFSIZE);
	memset(clientslen, 0, BUFSIZE);
	curclients = 0;
	// Create and bind listening socket.
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(9734);
	server_len = sizeof(server_address);
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
	/*memset(buf, '\0', BUFSIZE);
	gethostname(buf, BUFSIZE);
	printf("name = %s\n", buf);
	struct hostent *hostinfo;
	hostinfo = gethostbyname(buf);
	char **addrs = hostinfo->h_addr_list;
	while(*addrs) {
		printf("ip = %s\n", inet_ntoa(*(struct in_addr *)*addrs));
		addrs++;
	}
	*/
	// Listen to new incoming connections.
	listen(server_sockfd, 5);
	
	pthread_t thread_lobby;
	pthread_create(&thread_lobby, NULL, Thread_Lobby, NULL); 
	
	while (1) {
		struct sockaddr_in client_address;
		int client_len;
		int newclient;
		
		client_len = sizeof(client_address);
		newclient = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
		clientsfd[curclients] = newclient;
		clientslen[curclients] = client_len;
		clients_addr[curclients] = client_address;
		curclients++;
		printf("added client %d\n", newclient);
		perror(NULL);
		
	}
	
	pthread_join(thread_lobby, NULL);
	
	exit(0);
}
