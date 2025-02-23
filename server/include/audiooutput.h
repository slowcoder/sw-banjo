#pragma once

#include "mediaframe.h"

class Audiooutput {
public:
	virtual ~Audiooutput() = default;

	virtual int  open(int8_t bits,uint32_t rate) = 0;
	virtual void close(void) = 0;

	virtual int  fillBuffer(void *pData,int numSamples) = 0;
	virtual int  fillBuffer(mediaframe_t *pFrame) = 0;
	virtual void drainBuffer(void) = 0;

	virtual int  getSamplerate(void) = 0;
	virtual int  getBitdepth(void) = 0;
	virtual int  getBufferState(uint32_t *avail,uint32_t *size) = 0;

	virtual int  setVolume(uint8_t vol) = 0;
};
