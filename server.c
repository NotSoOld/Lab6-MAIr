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
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define BUFSIZE 512
#define MAXCHATS 32
#define MAX_USERS_PER_CHAT 32

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
};

struct chatinfo chats[MAXCHATS];
int numofchats;
int server_sockfd;

void AtInterruption(int sig)
{
	char *out;
	int i, j;
	
	out = "\033[1;7;99mSERVER:\033[0m Server was shut down. Connection closed.\n";
	for (i = 0; i < numofchats; i++) {
		for(j = 0; j < chats[i].curclients; j++) {
			write(chats[i].clients[j].fd, out, strlen(out));
			close(chats[i].clients[j].fd);
		}
	}
	close(server_sockfd);
	exit(0);
}

void *Thread_Lobby(void *arg)
{
	char buf[BUFSIZE];
	char mes[BUFSIZE+100];
	int i, j, k, m;
	
	m = (intptr_t)arg;
	while (1) {
		for (i = 0; i < chats[m].curclients; i++) {
			memset(buf, '\0', BUFSIZE);
			if(read(chats[m].clients[i].fd, buf, BUFSIZE) == -1)
				continue;
			if (strcmp(buf, "!exit\n") == 0) {
				printf("client %d exited\n", chats[m].clients[i].fd);
				close(chats[m].clients[i].fd);
				chats[m].curclients--;
				for (k = i; k < chats[m].curclients; k++)
					chats[m].clients[k] = chats[m].clients[k+1];
				continue;
			}
			for (j = 0; j < chats[m].curclients; j++)
				if (i != j) {
					memset(mes, '\0', BUFSIZE+100);
					sprintf(mes, "\033[1;7;38;5;%dm%s:\033[0m %s", 
							chats[m].clients[i].col, 
							chats[m].clients[i].name, buf);
					write(chats[m].clients[j].fd, mes, strlen(mes));
				}
		}
	}
	
	pthread_exit(NULL);
}

void main(void)
{
	int server_len;
	struct sockaddr_in server_address;
	char buf[BUFSIZE];
	char *out;
	
	signal(SIGINT, AtInterruption);
	numofchats = 0;
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
		struct clientinfo newclient;
		int client_len;
		int newclientfd;
		int i = 0;
		
		memset(&newclient, 0, sizeof(newclient));
		client_len = sizeof(client_address);
		newclientfd = accept(server_sockfd, 
						  (struct sockaddr *)&client_address, &client_len);
						  
		out = "\033[1;7;99mSERVER:\033[0m What's your nickname?\n";
		write(newclientfd, out, strlen(out));
		memset(buf, '\0', BUFSIZE);
		read(newclientfd, buf, 80);
		buf[strlen(buf)-1] = '\0';
		strcpy(newclient.name, buf);
		
		out = "\033[1;7;99mSERVER:\033[0m You may join one of existing chats";
		write(newclientfd, out, strlen(out));
		out = " or create a new one.\n\n\033[1mList of chats:\033[0m\n";
		write(newclientfd, out, strlen(out));
		if (numofchats == 0) {
			out = "\033[2m<no existing chats>\033[0m\n";
			write(newclientfd, out, strlen(out));
			i++;
		} else {
			for (i = 0; i < numofchats; i++) {
				memset(buf, '\0', BUFSIZE);
				sprintf(buf, "%d: %s\n", i+1, chats[i].title);
				write(newclientfd, buf, strlen(buf));
			}
		}
		write(newclientfd, "\n", 1);
		out = "R: Refresh list of chats\n";
		write(newclientfd, out, strlen(out));
		out = "C: Create new chat\n";
		write(newclientfd, out, strlen(out));
		out = "Q: Quit\n\n\033[1mYour choice? [chat index/R/C/Q]\033[0m\n";
		write(newclientfd, out, strlen(out));
		/*
		fcntl(newclientfd, F_SETFL, fcntl(newclientfd, F_GETFL) | O_NONBLOCK);
		srand(time(NULL));
		newclient.col = rand() % 255 + 1;
		newclient.fd = newclientfd;
		newclient.addrlen = client_len;
		newclient.addr = client_address;
		clients[curclients] = newclient;
		printf("Added client; fd = %d, name = %s, color = %d\n", 
				clients[curclients].fd, clients[curclients].name, 
				clients[curclients].col);
		curclients++;
		perror(NULL);
		*/
	}
	
	pthread_join(thread_lobby, NULL);
	
	exit(0);
}
