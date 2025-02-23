#pragma once

#include <stdint.h>

class TCP {
public:
	TCP();
	~TCP();

	int  connect(const char *pzAddr,uint16_t port);

	int  listen(uint16_t port);
	int  accept(int timeoutms = 100);

	void close(void);
	int  isConnected(void);

	int  recv(void *data,int len,uint32_t timeoutms);
	int  send(void *data,int len);

private:
	bool bIsServer,bIsConnected;
	int  s,ss;
};
