#pragma once

#include "shared/tcp.h"
#include "shared/util.h"

#include "mediaframe.h"

class AudioinputSP {
public:
	AudioinputSP();
	~AudioinputSP();

	int openPort(uint16_t port);
	virtual void close(void) = 0;

	virtual mediaframe_t *getNextFrame(void) = 0;
private:
	TCP *pTCP;
};
