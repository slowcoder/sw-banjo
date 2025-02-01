#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#include "error.h"

#include "tcp_server.h"

#define PORT_NUMBER 4800

static struct {
	int server_socket;
	int clientSock;
} me;

#if 0
static pthread_t thread_hdl;

#pragma pack(push,1)
typedef struct {
	uint16_t id;
	uint16_t len;
	uint8_t  data[256];
} netchunk_t;
#pragma pack(pop)

static void handleClient(int sock) {
	int done = 0;

	// New client, so flush the old stale stuff
	audio_queue_flush();
	audio_restart();

	while(!done) {
		ssize_t s;
		netchunk_t chunk;
		int qw,qf;

		audio_queue_getStats(&qw,&qf);

		chunk.id  = 0x01;
		chunk.len = 2;
		chunk.data[0] = qw;
		chunk.data[1] = qf;

		s = write(sock,&chunk,2+2+2);
		if( s != 6 ) {
			printf("Failed to write to tcp client: %li",s);
			done = 1;
		}
        usleep(200 * 1000);
	}
}
#endif

#if 0
static void *tcp_server_task(void *args) {
	int v;

//    printf("TCP Server task started. Sleeping forever\n");

	struct sockaddr_in clientAddress;
	struct sockaddr_in serverAddress;

	signal(SIGPIPE, SIG_IGN);

	// Create a socket that we will listen upon.
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( sock < 0 ) {
		printf("socket: %d %s", sock, strerror(errno));
		goto END;
	}

	v = 1;
	if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) ) {
		printf("Failed to setsockopt\n");
	}


	// Bind our server socket to a port.
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT_NUMBER);
	int rc  = bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc < 0) {
		printf("bind: %d %s", rc, strerror(errno));
		goto END;
	}

	// Flag the socket as listening for new connections.
	rc = listen(sock, 5);
	if (rc < 0) {
		printf("listen: %d %s", rc, strerror(errno));
		goto END;
	}

	printf("TCP Server started and listening\n");
	while (1) {
		// Listen for a new client connection.
		socklen_t clientAddressLength = sizeof(clientAddress);
		int clientSock = accept(sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
		if (clientSock < 0) {
			printf("accept: %d %s", clientSock, strerror(errno));
			goto END;
		}

		//printf("Accepted client %s",inet_ntoa(clientAddress.sin_addr.s_addr));
		printf("Accepted client %s\n",inet_ntoa(clientAddress.sin_addr));

		handleClient(clientSock);

		close(clientSock);
	}

END:
	printf("Fault in tcp-server task. I'm doomed..");
	//vTaskDelete(NULL); // Suicide
	return NULL;
}
#endif

err_code tcp_server_accept(int *s) {
	struct sockaddr_in clientAddress;

	// Listen for a new client connection.
	socklen_t clientAddressLength = sizeof(clientAddress);
	me.clientSock = accept(me.server_socket, (struct sockaddr *)&clientAddress, &clientAddressLength);
	if (me.clientSock < 0) {
		printf("accept: %d %s", me.clientSock, strerror(errno));
		goto END;
	}

	//printf("Accepted client %s",inet_ntoa(clientAddress.sin_addr.s_addr));
	printf("Accepted client %s\n",inet_ntoa(clientAddress.sin_addr));

	*s = me.clientSock;

	return ERROR_OK;
END:
	return ERROR_BASE;	
}

err_code tcp_server_init(void) {
	struct sockaddr_in serverAddress;

	// Ignore SIGPIPE, so that we get errors when
	// trying to send on a closed socket instead
	signal(SIGPIPE, SIG_IGN);

	// Create a socket that we will listen upon.
	me.server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( me.server_socket < 0 ) {
		printf("socket: %d %s", me.server_socket, strerror(errno));
		goto END;
	}

	int v = 1;
	if( setsockopt(me.server_socket, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) ) {
		printf("Failed to setsockopt\n");
	}


	// Bind our server socket to a port.
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT_NUMBER);
	int rc  = bind(me.server_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc < 0) {
		printf("bind: %d %s", rc, strerror(errno));
		goto END;
	}

	// Flag the socket as listening for new connections.
	rc = listen(me.server_socket, 5);
	if (rc < 0) {
		printf("listen: %d %s", rc, strerror(errno));
		goto END;
	}

	return ERROR_OK;
END:
	return ERROR_BASE;
}

int tcp_recv(void *pBuffer,int len,int timeoutms) {
	struct timeval tv;
	fd_set rset;
	int r;

	FD_ZERO(&rset);
	FD_SET(me.clientSock, &rset);

	tv.tv_sec  = timeoutms / 1000;
	tv.tv_usec = (timeoutms % 1000) * 1000;

	r = select(me.clientSock + 1, &rset, NULL, NULL, &tv);
	if( r > 0 ) {
		if( FD_ISSET(me.clientSock, &rset) ) {
			ssize_t rr;

			rr = read(me.clientSock,pBuffer,len);
			if( (r==1) && (rr==0) ) {
				return -1; // Hangup?
			}
			return rr;
		}
	} else if( r == 0 ) {
		return 0;
	}

	return -1;
}
