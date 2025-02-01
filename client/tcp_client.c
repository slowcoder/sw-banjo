#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "tcp_client.h"

static int sock;
static fd_set rset;

int tcp_connect(char *pzServer,uint16_t port) {

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, pzServer, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(port);

	// open a stream socket
	if( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
		printf("could not create socket\n");
		return -1;
	}

	// TCP is connection oriented, a reliable connection
	// **must** be established before any data is exchanged
	if (connect(sock, (struct sockaddr*)&server_address,
	            sizeof(server_address)) < 0) {
		printf("could not connect to server\n");
		return -2;
	}

	return 0;
}

int tcp_send(void *pBuffer,int len) {
	int r;

	r = send(sock,pBuffer,len,0);

	return r;
}

int tcp_recv(void *pBuffer,int len,int timeoutms) {
	struct timeval tv;
	int r;

	FD_ZERO(&rset);
	FD_SET(sock, &rset);

	tv.tv_sec  = timeoutms / 1000;
	tv.tv_usec = (timeoutms % 1000) * 1000;

	r = select(sock + 1, &rset, NULL, NULL, &tv);
	if( r > 0 ) {
		if( FD_ISSET(sock, &rset) ) {
			ssize_t r;

			r = read(sock,pBuffer,len);
			return r;
		}
	} else if( r == 0 ) {
		return 0;
	}

	return -1;
}
