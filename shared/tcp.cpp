#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>

#include "shared/log.h"

#include "shared/tcp.h"

TCP::TCP() {
	bIsServer = bIsConnected = false;
	ss = s = -1;

	// Ignore SIGPIPE, so that we get errors when
	// trying to send on a closed socket instead
	::signal(SIGPIPE, SIG_IGN);
}

TCP::~TCP() {
	if( ss > 0 ) ::close(ss);
	if( s  > 0 ) ::close(s);
}

int TCP::connect(const char *pzAddr,uint16_t port) {
	struct sockaddr_in server_address;
	int sock;


	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, pzAddr, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(port);

	// open a stream socket
	if( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
		LOGE("could not create socket");
		return -1;
	}

	// TCP is connection oriented, a reliable connection
	// **must** be established before any data is exchanged
	if( ::connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		LOGE("could not connect to server");
		return -2;
	}

	s = sock;
	bIsConnected = true;

	return 0;
}

int TCP::listen(uint16_t port) {
	struct sockaddr_in serverAddress;
	int v,rc;

	// Ignore SIGPIPE, so that we get errors when
	// trying to send on a closed socket instead
	signal(SIGPIPE, SIG_IGN);

	// Create a socket that we will listen upon.
	ss = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( ss < 0 ) {
		LOGE("socket: %d %s", ss, strerror(errno));
		goto END;
	}

	v = 1;
	if( setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) ) {
		LOGE("Failed to setsockopt");
	}


	// Bind our server socket to a port.
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);
	rc  = bind(ss, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc < 0) {
		LOGE("bind: %d %s", rc, strerror(errno));
		goto END;
	}

	// Flag the socket as listening for new connections.
	rc = ::listen(ss, 5);
	if (rc < 0) {
		LOGE("listen: %d %s", rc, strerror(errno));
		goto END;
	}

	return 0;
END:
	return -1;

}

int TCP::accept(int timeoutms) {
	struct sockaddr_in clientAddress;
	struct timeval tv;
	fd_set rset;
	int r;

	// See if there's any pending connections
	FD_ZERO(&rset);
	FD_SET(ss, &rset);

	tv.tv_sec  = timeoutms / 1000;
	tv.tv_usec = (timeoutms % 1000) * 1000;

	r = select(ss + 1, &rset, NULL, NULL, &tv);
	if( r > 0 ) {
		if( FD_ISSET(ss, &rset) ) {
			// Connection is pending
			LOG("Got pending connection");
		} else {
			return -1;
		}
	} else if( r == 0 ) {
		return -2;
	} else {
		LOGE("select() error..");
		return -3;
	}

	// Accept a new client connection.
	socklen_t clientAddressLength = sizeof(clientAddress);
	s = ::accept(ss, (struct sockaddr *)&clientAddress, &clientAddressLength);
	if( s < 0) {
		LOGE("accept: %d %s", s, strerror(errno));
		return -1;
	}

	LOG("Accepted client %s",inet_ntoa(clientAddress.sin_addr));

	return 0;
}

void TCP::close(void) {
	::close(s);
	s = -1;
}

int TCP::isConnected(void) {
	return bIsConnected;
}

int TCP::recv(void *data,int len,uint32_t timeoutms) {
	struct timeval tv;
	fd_set rset;
	int r;

	FD_ZERO(&rset);
	FD_SET(s, &rset);

	tv.tv_sec  = timeoutms / 1000;
	tv.tv_usec = (timeoutms % 1000) * 1000;

	r = select(s + 1, &rset, NULL, NULL, &tv);
	if( r > 0 ) {
		if( FD_ISSET(s, &rset) ) {
			ssize_t r;

			//r = read(s,data,len);
			r = ::recv(s,data,len,MSG_WAITALL);
			if( r < 0 ) bIsConnected = false;
			return r;
		}
	} else if( r == 0 ) {
		return 0;
	}

	return -1;
}

int TCP::send(void *data,int len) {
	int r;

	r = ::send(s,data,len,0);

	if( r < 0 ) bIsConnected = false;

	return r;
}
