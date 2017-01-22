#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_EVENTS 128

static int socket_svr_bind( char *port );

static int make_socket_nonblock( int sockfd );

static void daemonize();

int main(int argc, char *argv[])
{
	int sockfd, s;
	int epoll_fd;
	char *port;
	struct epoll_event ev;
	struct epoll_event *evs;
	if( argc < 2 ){
		fprintf(stderr, "Usage %s [port]\n", argv[0] );
		exit(1);
	}
	port = argv[1];
	
	//daemonize();

	sockfd = socket_svr_bind(port); printf("bind svr socket id:%d\n", sockfd);
	if( sockfd < 0 ){
		printf("%s\n", strerror(errno));
		abort();
	}
	s = make_socket_nonblock(sockfd);
	if( s < 0 ){
		printf("%s\n", strerror(errno));
		abort();
	}
	s = listen( sockfd, SOMAXCONN );
	if( s == -1 ){
		perror("bind");
		exit(1);
	}
	epoll_fd = epoll_create1(0);
	if( epoll_fd == -1 ){
		perror("epoll_create1");
		abort();
	}
	ev.data.fd = sockfd;
	ev.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev );
	if( s == -1 ){
		perror("epoll_ctl");
		abort();
	}
	evs = calloc(MAX_EVENTS, sizeof ev);
	/** Event loop */
	while(1)
	{
		int n,i;
		n = epoll_wait( epoll_fd, evs, MAX_EVENTS, -1 );//blocking for getting  readable sockets
		for( i = 0; i < n; i++ )
		{
			if( (evs[i].events & EPOLLERR ) || ( evs[i].events & EPOLLHUP ) || ( !evs[i].events & EPOLLIN ) ){
				fprintf(stderr, "epoll_wait error\n");
				close(evs[i].data.fd);
				continue;
			} else if( sockfd == evs[i].data.fd ){ //svr bind socket
				while(1)
				{
					struct sockaddr_in in_addr;
					socklen_t in_len;
					int socket_in;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					in_len = sizeof(in_addr);
					socket_in = accept( sockfd, (struct sockaddr*)&in_addr, &in_len );
					if( socket_in == -1 ){
						if( errno == EAGAIN || errno == EWOULDBLOCK ){
							break;
						}else{
							printf("accept error[%d]:%s\n", errno, strerror(errno));
							break;
						}
					}
					s = getnameinfo( (const struct sockaddr *)&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST|NI_NUMERICSERV);
					if( s== 0 ){
						printf("accepted connection on descriptor:%d, ip:%s, port:%s\n", socket_in, hbuf, sbuf);
					}
					s = make_socket_nonblock(socket_in);
					if( s == -1 ){
						perror("make_socket_nonblock");
						abort();
					}
					ev.data.fd = socket_in;
					ev.events = EPOLLIN|EPOLLET;
					s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_in, &ev);
					if( s == -1 ){
						perror("epoll_ctl");
						abort();
					}
				}
				continue;
			}else{ //accept client socket
				int done = 0;
				while(1)
				{
					ssize_t count;
					char buf[512];
					count = read(evs[i].data.fd, buf, sizeof(buf) );
					if( count == -1 ){
						/* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
						if( errno != EAGAIN ){
							perror("socket read");
							done = 1;
						}
						break;
					}else if( count == 0 ){
						/* End of file. The remote has closed the
                         connection. */
						done = 1;
						break;
					}//if we get count == sizeof(buf) there were more data in the socket waiting to read!
					//else if( count == sizeof(buf) ){ continue; }

					printf("content from remote client:%s\n", buf);
					write(evs[i].data.fd, buf, strlen(buf));
				}
				if( done ){
					printf("connection close by remote socket:%d\n", evs[i].data.fd);
					close(evs[i].data.fd);
				}
			}
		}
	}
	free(evs);
	close(epoll_fd);
	close(sockfd);
	return 0;
}

static void daemonize()
{
	int pid,n;
	pid = fork();
	if( pid ){
		exit(0);
	}else if( pid < 0 ){
		printf("fork error\n");
		exit(1);
	}
	setsid();
	if( pid = fork() ){
		exit(0);
	}else if( pid < 0 ){
		printf("fork child proccess failed\n");
		exit(1);
	}
	for( n = 0; n < NOFILE; n++ ){
		close(n);
	}
	chdir("/tmp");
	umask(0);
	return;
}
static int make_socket_nonblock( int sockfd )
{
	int flag, s;
	flag = fcntl(sockfd, F_GETFL, 0);
	if( flag == -1 ){
		perror("fcntl");
		return -1;
	}
	flag = flag | O_NONBLOCK;
	s = fcntl(sockfd, F_SETFL, flag );
	if( s == -1 ){
		perror("fcntl");
		return -2;
	}
	return 0;
}

static int socket_svr_bind( char *port )
{
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s, sockfd;

	/** 初始化变量结构体 */
	memset( &hints, 0, sizeof(struct addrinfo) );
	hints.ai_family = AF_INET;     /* Return IPv4 and IPv6 choices */
  	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
  	hints.ai_flags = 0;     /* All interfaces */

  	s = getaddrinfo( NULL, port, &hints, &result);
  	if( s != 0 ){
  		fprintf(stderr, "getaddrinfo error:%s\n", gai_strerror(s) );
  		return -1;
  	}
  	for( rp = result; rp != NULL; rp = rp->ai_next )
  	{
  		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_flags );
  		if( sockfd == -1 ){
  			continue;
  		}
  		int reuse = 1;
  		setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int) );
  		s = bind( sockfd, rp->ai_addr, rp->ai_addrlen );
  		if( s == 0 ){
  			break;
  		}
  		close(sockfd);
  	}
  	if( rp == NULL ){
  		fprintf(stderr, "can not bind socket\n");
  		return -2;
  	}
  	freeaddrinfo(result);
  	return sockfd;
}