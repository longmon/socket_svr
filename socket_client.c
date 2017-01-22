#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int port, sockfd;
	char *msg = NULL;
	int msglen = 0;

	if( argc < 4 ){
		printf("Usage %s [ip] [port]\n", argv[0] );
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0 );
	port = atoi(argv[2]);
	server = gethostbyname(argv[1]);
	if( server == NULL ){
		perror("gethostbyname");
		abort();
	}
	bzero(&serv_addr, sizeof(serv_addr) );
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&(serv_addr.sin_addr.s_addr), server->h_length);
	serv_addr.sin_port = htons(port);

	int flag = fcntl( sockfd, F_GETFL, 0 );
	flag = flag|O_NONBLOCK;
	fcntl( sockfd, F_SETFL, flag );
	if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr) ) < 0 ){
		if( errno != EINPROGRESS ){
			perror("connect");
			exit(1);
		}
	}
	msg = argv[3];
	msglen = strlen(msg);
	if( write(sockfd, msg, msglen) < 0 ){
		perror("socket write");
		exit(1);
	}
	char rcvbuf[512];
	if( recv(sockfd, rcvbuf, 512, 0) < 0 ){
		perror("socket recv");
		exit(1);
	}
	printf("recv from socket:%s\n", rcvbuf);

	return 0;
}