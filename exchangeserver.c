#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h> 
#include<errno.h>
#include<signal.h>
#include<netdb.h>

#define CONNMAX 1000
#define BYTES 1024
#define RESPONSE 4096
#define MESSAGE_SIZE 2048

#define DEF_PORT	"5000"

#define DEF_PREFIX		"daemon/"

#define DEF_RUN_START	"nohup /usr/bin/php  /opt/WWW/"
#define DEF_RUN_END	"/content_new/bin/console "

#define DEF_RUN_MODE_PROD	" --env=prod "
#define DEF_RUN_MODE_DEV	" --env=dev "
#define DEF_RUN_FLUSH		" > /dev/null  2>&1"

char *PORT;
char *RUN_START;
char *RUN_END;
char *RUN_MODE;
char *PREFIX;

int listenfd, client_sock, *new_sock;
pthread_t sniffer_thread;

void open_server(char *);
void close_server(int);
void *connection_handler(void *);

int main(int argc, char* argv[]) {
	struct sockaddr_in clientaddr;
	socklen_t addrlen;
	char ch;
	pid_t pid_server;
	int is_test = 0;
	int is_background = 0;
	int status = 0;
	
	while ((ch = getopt (argc, argv, "p:s:e:m:k:tb")) != -1)
		switch (ch) {
		case 'p':
			PORT = (char*) malloc(strlen(optarg)+1);
			strcpy(PORT,optarg);
		break;
		case 's':
			RUN_START = (char*) malloc(strlen(optarg)+1);
			strcpy(RUN_START,optarg);
		break;
		case 'e':
			RUN_END = (char*) malloc(strlen(optarg)+1);
			strcpy(RUN_END,optarg);
		break;
		case 'm':
			RUN_MODE = (char*) malloc(strlen(optarg)+1);
			strcpy(RUN_MODE,optarg);
		break;
		case 'k':
			PREFIX = (char*) malloc(strlen(optarg)+1);
			strcpy(PREFIX,optarg);
		break;
		case 't':
			is_test = 1;
		break;
		case 'b':
			is_background = 1;
		break;
		case '?':
			fprintf(stderr,"Wrong arguments given!!!\n");
			exit(1);
		break;
		default:
			exit(1);
		}

	if (!is_test) {
		if (RUN_MODE == NULL){
			RUN_MODE = (char*) malloc(strlen(DEF_RUN_MODE_PROD)+1);
			strcpy(RUN_MODE,DEF_RUN_MODE_PROD);
		}
	} else {
		if (RUN_MODE != NULL) free(RUN_MODE);
		RUN_MODE = (char*) malloc(strlen(DEF_RUN_MODE_DEV)+1);
		strcpy(RUN_MODE,DEF_RUN_MODE_DEV);
	}
	
	if (RUN_START == NULL){
		RUN_START = (char*) malloc(strlen(DEF_RUN_START)+1);
		strcpy(RUN_START,DEF_RUN_START);
	}
	
	if (RUN_END == NULL){
		RUN_END = (char*) malloc(strlen(DEF_RUN_END)+1);
		strcpy(RUN_END,DEF_RUN_END);
	}
	if (PORT == NULL){
		PORT = (char*) malloc(strlen(DEF_PORT)+1);
		strcpy(PORT,DEF_PORT);
	}
	
	if (PREFIX == NULL){
		PREFIX = (char*) malloc(strlen(DEF_PREFIX)+1);
		strcpy(PREFIX,DEF_PREFIX);
	}
	
	open_server(PORT);
	
	printf("Server started at port [%s]. Run script [%s[SERVER]%s [command] %s %s]. Search prefix: [%s]\n",PORT, RUN_START, RUN_END, RUN_MODE, DEF_RUN_FLUSH, PREFIX);
	printf("Server at mode background[%d], test[%d]\n",is_background,is_test);
	switch(pid_server=fork()) {
		case -1:
			perror ("server fork error");
			exit(1);
		case 0:
			while (1) {
				addrlen = sizeof(clientaddr);
				client_sock = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);
				new_sock = malloc(sizeof(int));
				*new_sock = client_sock;
				if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0) {
					perror("could not create thread");
					return 1;
				}
			}
			exit(0);
		default:
			while(read(0, &ch, sizeof(ch)) > 0){
				if (ch == '0') {
					break;
				}
			}
			if (is_background) {
				waitpid(pid_server,&status,0);
			} else {
				kill(pid_server, SIGKILL);
			}
			close_server(listenfd);
			free(RUN_START);
			free(RUN_END);
			free(RUN_MODE);
			free(PREFIX);
			free(PORT);
	}
	
	return 0;
}

//start server
void open_server(char *port) {
	struct addrinfo hints, *res, *p;
	int enable=1;

	printf("start Init Server\n");
	// getaddrinfo for host
	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo( NULL, port, &hints, &res) != 0) {
		perror ("getaddrinfo() error");
		exit(1);
	}
// socket and bind
	for (p = res; p!=NULL; p=p->ai_next) {
		listenfd = socket (p->ai_family, p->ai_socktype, 0);
		if (listenfd == -1) continue;
		if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&enable,sizeof(enable)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
	}
	if (p==NULL) {
		perror ("socket() or bind()");
		exit(1);
	}
	freeaddrinfo(res);
// listen for incoming connections
	if ( listen (listenfd, 1000000) != 0 ) {
		perror("listen() error");
		exit(1);
	}
	printf("end Init Server\n");
}

void close_server(int fd) {
	if (fd >= 0) {
		if (shutdown(fd, SHUT_RDWR) < 0)
			if (errno != ENOTCONN && errno != EINVAL)
				perror("shutdown");
		if (close(fd) < 0)
			perror("close");
	}
	printf("close Server\n");
}


void *connection_handler(void *socket_desc)
{
	int sock = *(int*)socket_desc;
	int read_size;
	char *message , client_message[MESSAGE_SIZE], *reqline[3],  *response;
	char *prefix, *server, *command;
	
	response = malloc( (sizeof (char) * MESSAGE_SIZE)+1 );
	read_size=recv(sock , client_message , MESSAGE_SIZE , 0);
	if (read_size<0)
		fprintf(stderr,("recv() error\n"));
	else if (read_size==0)
		fprintf(stderr,"Client disconnected upexpectedly.\n");
	else {
		reqline[0] = strtok (client_message, " \t\n");
		if ( strncmp(reqline[0], "GET\0", 4)==0 ) {
			reqline[1] = strtok (NULL, " \t");
			reqline[2] = strtok (NULL, " \t\n");
			if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 ) {
				write(sock, "HTTP/1.0 400 Bad Request\n", 25);
			} else {
				if (strncmp(reqline[1], "/\0", 2)!=0 ) {
					reqline[1]++;
					if(!strncmp(PREFIX, reqline[1], strlen(PREFIX))){
						prefix = strtok(reqline[1], "/");
						if(prefix) {
							server = strtok(NULL, "/");
							if(server) {
								command = strtok(NULL, "/");
								write(sock, "HTTP/1.0 200 OK\n", 17);
								strcpy(response, RUN_START);
								strcpy(&response[strlen(response)], server);
								strcpy(&response[strlen(response)], RUN_END);
								strcpy(&response[strlen(response)], command);
								strcpy(&response[strlen(response)], RUN_MODE);
								strcpy(&response[strlen(response)], DEF_RUN_FLUSH);
								printf("%s\n",response);
								system(response);
							}
						}
					} else {
						write(sock, "HTTP/1.0 403 Forbidden\n", 23);
					}
				} else {
					write(sock, "HTTP/1.0 403 Forbidden\n", 23);
				}
			}
		}
	}
	shutdown (sock, SHUT_RDWR);
	close(sock);
	free(socket_desc);
	
	return 0;
}