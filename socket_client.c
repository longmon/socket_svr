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
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 100

int main(int argc, char *argv[])
{
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int port, sockfd;
	char *msg = NULL;
	int msglen = 0;
	int fds[10];

	int epoll_fd = 0;
	struct epoll_event event[10];
	struct epoll_event *events;

	epoll_fd = epoll_create1(0);
	if( epoll_fd == -1 ){
		printf("create epoll fd error [%d]: %s\n", errno, strerror(errno));
		exit(1);
	}


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
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt( sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	int m =0;
	for( m = 0; m < 10; m++ ){
		fds[m] = dup( sockfd );
		event[m].data.fd = fds[0];
		event[m].events = EPOLLOUT|EPOLLET;
		epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fds[m], &event[m]);
	}
	close(sockfd);
	msg = argv[3];
	msglen = strlen(msg);
	events = calloc(MAX_EVENTS, sizeof(struct epoll_event) );
	int n = epoll_wait( epoll_fd, events, MAX_EVENTS, -1 ) ;
	if( n > 0 ){
		int j = 0;
		for( j = 0; j < n; j++ ){
			if( events[j].events & EPOLLERR || events[j].events & EPOLLHUP || (!events[j].events & EPOLLOUT ) ){
				fprintf(stderr, "epoll_wait error[%d]:%s\n", errno, strerror(errno));
				close(events[j].data.fd);
				continue;
			}else{
				flag = fcntl( events[j].data.fd, F_GETFL, 0);
				fcntl(events[j].data.fd,F_SETFL,flag&~O_NONBLOCK);
				if( write( events[j].data.fd, msg, msglen) < 0 ){
					printf("write error[%d]:%s\n", errno, strerror(errno));
					close(events[j].data.fd);
					epoll_ctl( epoll_fd, events[j].data.fd, EPOLL_CTL_DEL, 0);
				}
				char rcvbuf[512];
				int retval;
				while( retval = recv( events[j].data.fd, rcvbuf, 512, 0) < 0 ){
					if( retval == -1 && EAGAIN != errno )
					{
						printf("recv error[%d]:%s\n",errno, strerror(errno));
						break;
					}else
					{
						printf("retval:%d, error[%d]:%s\n", retval, errno, strerror(errno));
					}
				}
				printf("recv from socket:%s\n", rcvbuf);
				break;
			}
		}
	}else if( n == -1 ){
		printf("epoll_wait_error[%d]: %s\n", errno, strerror(errno) );
		return -1;
	}else{
		printf("epoll wait timeout!\n");
	}
	return 0;
}