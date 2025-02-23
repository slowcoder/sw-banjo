#pragma once

#include <stdint.h>

class BURIPlayer {
public:
	BURIPlayer();
	~BURIPlayer();

	// For clients
	int  connect(const char *pzServer,uint16_t port);
	void disconnect();
	int  isConnected();

	int  play(const char *pzURI);
	int  stop(void);

};
