#pragma once

#include "mediaframe.h"

class Audioinput {
public:
	virtual ~Audioinput() = default;

//	virtual int  openstream(const char *pzURI) = 0;
	virtual void close(void) = 0;

	virtual mediaframe_t *getNextFrame(void) = 0;
};
