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
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>

#define BUFSIZE 512
#define MAXCHATS 32
#define MAX_USERS_PER_CHAT 32
#define MAX_USERS_IN_LOBBY 32
#define SERV "\033[1;7;99mSERVER:\033[0m "

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
};

struct chatinfo chats[MAXCHATS];
struct chatinfo lobby;
int numofchats;
int server_sockfd;
int maxconnectedfd;

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

void WriteGreeting(int newclientfd)
{
	int i = 0;
	char *out;
	char buf[BUFSIZE];
	
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
}

void *Thread_Chat(void *arg)
{
	char buf[BUFSIZE];
	char mes[BUFSIZE+100];
	int i, j, k, m;
	
	m = (intptr_t)arg;
	while (1) {
		if (chats[m].curclients == 0)
			break;
		for (i = 0; i < chats[m].curclients; i++) {
			memset(buf, '\0', BUFSIZE);
			if(read(chats[m].clients[i].fd, buf, BUFSIZE) == -1)
				continue;
			if (strcmp(buf, "!exit\n") == 0) {
				printf("Client %s (fd %d) exited\n", 
						chats[m].clients[i].name, 
						chats[m].clients[i].fd);
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
	
	// Here goes chat deleting.
	
	pthread_exit(NULL);
}

void *Thread_Lobby(void *arg)
{
	int i, j, k, res;
	fd_set testfds;
	char buf[BUFSIZE];
	char *out;
	
	while (1) {
		testfds = lobby.clientfds;
		res = select(maxconnectedfd+1, &testfds, NULL, NULL, NULL);
		if (res < 1)
			perror("select in lobby");
		else {
			for (i = 0; i <= maxconnectedfd; i++) {
				if (FD_ISSET(i, &testfds)) {
					ioctl(i, FIONREAD, &res);
					if (res <= 0)
						continue;
					read(i, buf, BUFSIZE);
					res = 0;
					res = (int)strtol(buf, NULL, 10);
					// Find client by fd among our clients. 'j' == client index.
						for (j = 0; j < lobby.curclients; j++)
								if (lobby.clients[j].fd == i)
									break;
					// If conversion was successful...
					if (res != 0) {
						// Add client to chat with number 'res'-1.
						FD_CLR(i, &(lobby.clientfds));
						FD_SET(i, &(chats[res-1].clientfds));
						
						// Add to chat and erase from lobby.
						chats[res-1].clients[chats[res-1].curclients] = lobby.clients[j];
						chats[res-1].curclients++;
						lobby.curclients--;
						for(k = j; k < lobby.curclients; k++)
								lobby.clients[k] = lobby.clients[k+1];
						memset(buf, '\0', BUFSIZE);
						sprintf(buf, "%sYou joined chat \033[1m%s\033[0m\n", SERV, chats[res-1].title);
						write(i, buf, strlen(buf));
					} else {
					// In other cases there was a char (or garbage) in 'buf'.
						if (strcmp(buf, "R\n") == 0 || strcmp(buf, "r\n")) {
							out = "\n\n\n";
							write(i, out, strlen(out));
							WriteGreeting(i);
						} else if (strcmp(buf, "C\n") == 0 || strcmp(buf, "c\n")) {
							// Start a dialog to create a chat.
							struct chatinfo newchat;
							memset(&newchat, 0, sizeof(newchat));
							out = "\033[1;7;99mSERVER:\033[0m Enter chat title:\n";
							write(i, out, strlen(out));
							memset(buf, '\0', BUFSIZE);
							read(i, buf, 80);
							buf[strlen(buf)-1] = '\0';
							strcpy(newchat.title, buf);
							newchat.curclients = 1;
							// Add client to new chat and erase from lobby.
							FD_SET(i, &(newchat.clientfds));
							FD_CLR(i, &(lobby.clientfds));
							newchat.clients[0] = lobby.clients[j];
							lobby.curclients--;
							for(k = j; k < lobby.curclients; k++)
								lobby.clients[k] = lobby.clients[k+1];
							chats[numofchats] = newchat;
							numofchats++;
							memset(buf, '\0', BUFSIZE);
							sprintf(buf, "%sYou created chat \033[1m%s\033[0m\n", SERV, newchat.title);
							write(i, buf, strlen(buf));
						} else if (strcmp(buf, "Q\n") == 0 || strcmp(buf, "q\n")) {
							// Close connection (erase info) and terminate client.
							out = "!term";
							write(i, out, strlen(out));
							FD_CLR(i, &(lobby.clientfds));
							close(i);
							lobby.curclients--;
							for(k = j; k < lobby.curclients; k++)
								lobby.clients[k] = lobby.clients[k+1];
						} else {
							out = "Cannot understand your choice. Repeat, please.\n\n\n";
							write(i, out, strlen(out));
							WriteGreeting(i);
						}
					}
				}
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
	
	// Initialize things.
	signal(SIGINT, AtInterruption);
	numofchats = 0;
	memset(&lobby, 0, sizeof(lobby));
	strcpy(lobby.title, "Lobby");
	FD_ZERO(&lobby.clientfds);
	maxconnectedfd = 0;
	// Create and bind listening socket.
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sockfd == -1) {
		perror("Failed to create server socket");
		exit(1);
	}
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(9734);
	server_len = sizeof(server_address);
	if (bind(server_sockfd, (struct sockaddr *)&server_address, server_len) == -1) {
		perror("Failed to bind server socket");
		exit(2);
	}
	// Output address of the server.
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
	if (listen(server_sockfd, 5) == -1)
		perror("Failed to listen server socket"); 
	
	pthread_t thread_lobby;
	if (pthread_create(&thread_lobby, NULL, Thread_Lobby, NULL) != 0) {
		perror("Failed to create lobby thread");
		exit(3);
	}
	
	while (1) {
		struct sockaddr_in client_address;
		struct clientinfo newclient;
		int client_len;
		int newclientfd;
		int i = 0;
		
		// Add new connected client.
		memset(&newclient, 0, sizeof(newclient));
		client_len = sizeof(client_address);
		newclientfd = accept(server_sockfd, 
						  (struct sockaddr *)&client_address, &client_len);
		if (newclientfd == -1) {
			perror("Failed to accept incoming connection");
			continue;
		}
					  
		out = "\033[1;7;99mSERVER:\033[0m What's your nickname?\n";
		write(newclientfd, out, strlen(out));
		memset(buf, '\0', BUFSIZE);
		read(newclientfd, buf, 80);
		buf[strlen(buf)-1] = '\0';
		strcpy(newclient.name, buf);
		srand(time(NULL));
		newclient.col = rand() % 255 + 1;
		newclient.fd = newclientfd;
		newclient.addrlen = client_len;
		newclient.addr = client_address;
		lobby.clients[lobby.curclients] = newclient;
		FD_SET(newclientfd, &lobby.clientfds);
		printf("Added client; fd = %d, name = %s, color = %d\n", 
				lobby.clients[lobby.curclients].fd,
				lobby.clients[lobby.curclients].name,
				lobby.clients[lobby.curclients].col);
		lobby.curclients++;
		if (newclientfd > maxconnectedfd)
			maxconnectedfd = newclientfd;
		
		// Write greeting to client. Other work will be held by lobby thread.
		WriteGreeting(newclientfd);
	
	}
	
	if (pthread_join(thread_lobby, NULL) != 0) {
		perror("Failed to join lobby thread");
		exit(3);
	}
	
	exit(0);
}
