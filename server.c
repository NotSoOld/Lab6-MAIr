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
	int maxfd;
};

struct chatinfo chats[MAXCHATS];
struct chatinfo lobby;
int numofchats;
int server_sockfd;

void AtInterruption(int sig)
{
	char buf[BUFSIZE];
	int i, j;
	
	memset(buf, '\0', BUFSIZE);
	sprintf(buf, "%sServer was shut down. Connection closed.\n", SERV);
	for (i = 0; i < numofchats; i++) {
		for(j = 0; j < chats[i].curclients; j++) {
			write(chats[i].clients[j].fd, buf, strlen(buf));
			close(chats[i].clients[j].fd);
		}
	}
	for(j = 0; j < lobby.curclients; j++) {
			write(lobby.clients[j].fd, buf, strlen(buf));
			close(lobby.clients[j].fd);
		}
	close(server_sockfd);
	exit(0);
}

void InitChats()
{
	int i;
	
	memset(&lobby, 0, sizeof(lobby));
	strcpy(lobby.title, "Lobby");
	FD_ZERO(&lobby.clientfds);
	
	for (i = 0; i < MAXCHATS; i++) {
		memset(&(chats[i]), 0, sizeof(chats[i]));
		chats[i].curclients = -1;
		FD_ZERO(&chats[i].clientfds);
	}
}

void WriteGreeting(int newclientfd)
{
	int i = 0;
	char *out;
	char buf[BUFSIZE];
	
	memset(buf, '\0', BUFSIZE);
	sprintf(buf, "%sYou may join one of existing chats or create a new one.\n\n", SERV);
	write(newclientfd, buf, strlen(buf));
	out = "\033[1mList of chats:\033[0m\n";
	write(newclientfd, out, strlen(out));
	if (numofchats == 0) {
		out = "\033[2m<no existing chats>\033[0m\n";
		write(newclientfd, out, strlen(out));
		i++;
	} else {
		for (i = 0; i < numofchats;) {
			if (chats[i].curclients < 1)
				continue;
			memset(buf, '\0', BUFSIZE);
			sprintf(buf, "%d: %s [%d/%d]\n", i+1, chats[i].title, 
						chats[i].curclients, MAX_USERS_PER_CHAT);
			write(newclientfd, buf, strlen(buf));
			i++;
		}
	}
	write(newclientfd, "\n", 1);
	out = "R: Refresh list of chats\nC: Create new chat\n";
	write(newclientfd, out, strlen(out));
	out = "Q: Quit\n\n\033[1mYour choice? [chat index/R/C/Q]\033[0m\n";
	write(newclientfd, out, strlen(out));
}

void EraseClientFromChat(struct chatinfo *chat, int start)
{
	int i;
	
	FD_CLR(chat->clients[start].fd, &chat->clientfds);
	chat->curclients--;
	for (i = start; i < chat->curclients; i++)
		chat->clients[i] = chat->clients[i+1];
}

void *Thread_Chat(void *);

void CreateChat(struct clientinfo creator)
{
	int i;
	char buf[BUFSIZE];
	struct chatinfo newchat;
	pthread_t chatthr;
	
	memset(&newchat, 0, sizeof(newchat));
	memset(buf, '\0', BUFSIZE);
	sprintf(buf, "%sEnter chat title:\n", SERV);
	write(creator.fd, buf, strlen(buf));
	memset(buf, '\0', BUFSIZE);
	read(creator.fd, buf, 80);
	buf[strlen(buf)-1] = '\0';
	strcpy(newchat.title, buf);
	newchat.curclients = 1;
	/* Add client to new chat. */
	FD_SET(creator.fd, &newchat.clientfds);
	newchat.maxfd = creator.fd;
	newchat.clients[0] = creator;
	memset(buf, '\0', BUFSIZE);
	sprintf(buf, "%sYou created chat \033[1m%s\033[0m\n", SERV, newchat.title);
	write(creator.fd, buf, strlen(buf));
	for (i = 0; i < MAXCHATS; i++)
		if (chats[i].curclients == -1) {
			chats[i] = newchat;
			break;
		}
	numofchats++;
	/* Create new thread for the chat. */
	if (pthread_create(&chatthr, NULL, Thread_Chat, (void *)(intptr_t)i) != 0) {
		perror("Failed to create a thread for new chat");
		return;
	}
	pthread_detach(chatthr);
}

void *Thread_Chat(void *arg)
{
	char buf[BUFSIZE];
	char mes[BUFSIZE+100];
	int i, j, k, m, id;
	fd_set testset;
	struct timeval tm;
	
	id = (intptr_t)arg;
	while (1) {
		if (chats[id].curclients == 0)
			break;
			
		tm.tv_sec = 1;
		tm.tv_usec = 0; 
		testset = chats[id].clientfds;
		m = select(chats[id].maxfd+1, &testset, NULL, NULL, &tm);
		if (m < 1)
			continue;
			
		for (i = 0; i <= chats[id].maxfd; i++) {
			printf("maxfd = %d, id = %d, curclients = %d\n", chats[id].maxfd, id, chats[id].curclients);
			if (FD_ISSET(i, &testset)) {
				ioctl(i, FIONREAD, &m);
				if (m <= 0)
					continue;
				memset(buf, '\0', BUFSIZE);
				read(i, buf, BUFSIZE);
				write(1, buf, sizeof(buf));
				/* We need to find this client in the list by fd ('k' equals index). */
				for (k = 0; k < chats[id].curclients; k++)
					if (chats[id].clients[k].fd == i)
						break;
				if (strcmp(buf, "!exit\n") == 0) {
					/* Send some messages. */
					printf("Client %s (fd %d) leaved\n", 
							chats[id].clients[k].name, 
							chats[id].clients[k].fd);
					memset(buf, '\0', BUFSIZE);
					sprintf(buf, "%s\033[1;7;38;5;%dm%s\033[0m leaved this chat\n", 
							SERV, chats[id].clients[k].col, 
							chats[id].clients[k].name);
					for (j = 0; j < chats[id].curclients; j++)
						if (j != k)
							write(chats[id].clients[j].fd, buf, strlen(buf));
					/* Erase client from chat and add him to lobby. */
					FD_SET(i, &(lobby.clientfds));
					if (i > lobby.maxfd)
						lobby.maxfd = i;
					lobby.clients[lobby.curclients] = chats[id].clients[k];
					lobby.curclients++;
					j = chats[id].clients[k].fd;
					EraseClientFromChat(&chats[id], k);
					WriteGreeting(j);
					continue;
				}
				/* Format and send message to everyone in this chat. */
				for (j = 0; j < chats[id].curclients; j++) {
					if (j != k) {
						memset(mes, '\0', BUFSIZE+100);
						sprintf(mes, "\033[1;7;38;5;%dm%s:\033[0m %s", 
								chats[id].clients[k].col, 
								chats[id].clients[k].name, buf);
						write(chats[id].clients[j].fd, mes, strlen(mes));
					}
				}
			}
		}
	}
	
	/* Here goes chat deletion: mark this chat as creatable. */
	numofchats--;
	chats[id].curclients = -1;
	
	pthread_exit(NULL);
}

void *Thread_Lobby(void *arg)
{
	int i, j, k, l, res;
	fd_set testfds;
	char buf[BUFSIZE];
	char *out;
	struct timeval tm;
	
	while (1) {
		if (lobby.curclients == 0) {
			sleep(1);
			continue;
		}
		tm.tv_sec = 1;
		tm.tv_usec = 0; 
		testfds = lobby.clientfds;
		perror("before select");
		res = select(lobby.maxfd+1, &testfds, NULL, NULL, &tm);
		if (res < 1)
			continue;
		else {
			perror("after select");
			for (i = 0; i <= lobby.maxfd; i++) {
				if (FD_ISSET(i, &testfds)) {
					ioctl(i, FIONREAD, &res);
					if (res <= 0)
						continue;
					perror("Trying to read");
					memset(buf, '\0', BUFSIZE);
					read(i, buf, BUFSIZE);
					/* Find client by fd among our clients. 'j' equals index. */
					for (j = 0; j < lobby.curclients; j++)
							if (lobby.clients[j].fd == i)
								break;

					res = 0;
					res = (int)strtol(buf, NULL, 10);
					perror("converted res");
					
					/* If conversion was successful... */
					if (res != 0) {
						perror("res was number");
						/* Find chat (because 'res' is not a valid index) */
						l = 0;
						for (k = 0; k < MAXCHATS; k++) {
							if (chats[k].curclients != -1) {
								l++;
								if (l == res)
									break;
							}
						}
						
						/* Add client to chat with number 'k'. */
						FD_SET(i, &chats[k].clientfds);
						printf("maxfd = %d, i = %d, res = %d\n", chats[k].maxfd, i, res);
						if (i > chats[k].maxfd)
							chats[k].maxfd = i;
						/* Add to chat and erase from lobby. */
						chats[k].clients[chats[k].curclients] = lobby.clients[j];
						chats[k].curclients++;
						printf("curclients = %d\n", chats[k].curclients);
						EraseClientFromChat(&lobby, j);
						/* Send some messages. */
						memset(buf, '\0', BUFSIZE);
						sprintf(buf, "%sYou joined chat \033[1m%s\033[0m\n", SERV, chats[k].title);
						write(i, buf, strlen(buf));
						memset(buf, '\0', BUFSIZE);
						sprintf(buf, "%s\033[1;7;38;5;%dm%s\033[0m joined this chat\n", 
								SERV, lobby.clients[j].col, 
								lobby.clients[j].name);
						for (l = 0; l < chats[k].curclients; l++)
							if (chats[k].clients[l].fd != i)
								write(chats[k].clients[l].fd, buf, strlen(buf));
					} else {
						// In other cases there was a char command (or garbage) in 'buf'.
						perror("res was char");
						if (strcmp(buf, "R\n") == 0 || strcmp(buf, "r\n") == 0) {
							out = "\n\n\n";
							write(i, out, strlen(out));
							WriteGreeting(i);
						} else if (strcmp(buf, "C\n") == 0 || strcmp(buf, "c\n") == 0) {
							/* Start a dialog to create a chat. */
							CreateChat(lobby.clients[j]);
							/* Erase chat creator from the lobby. */
							EraseClientFromChat(&lobby, j);
						} else if (strcmp(buf, "Q\n") == 0 || strcmp(buf, "q\n") == 0) {
							// Close connection (erase info) and terminate client.
							out = "!term";
							write(i, out, strlen(out));
							EraseClientFromChat(&lobby, j);
							close(i);
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
	
	// Initialize things.
	signal(SIGINT, AtInterruption);
	numofchats = 0;
	InitChats();
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
		
		// Add new connected client.
		memset(&newclient, 0, sizeof(newclient));
		client_len = sizeof(client_address);
		newclientfd = accept(server_sockfd, 
						  (struct sockaddr *)&client_address, &client_len);
		if (newclientfd == -1) {
			perror("Failed to accept incoming connection");
			continue;
		}
		
		memset(buf, '\0', BUFSIZE);
		sprintf(buf, "%sWhat's your nickname?\n", SERV);
		write(newclientfd, buf, strlen(buf));
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
		if (newclientfd > lobby.maxfd)
			lobby.maxfd = newclientfd;
		
		/* Write greeting to client. Other work will be held by lobby thread. */
		WriteGreeting(newclientfd);
	
	}
	
	if (pthread_join(thread_lobby, NULL) != 0) {
		perror("Failed to join lobby thread");
		exit(3);
	}
	
	exit(0);
}
