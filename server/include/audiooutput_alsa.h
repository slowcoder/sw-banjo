#pragma once 

#include "audiooutput.h"

class AudiooutputALSA final: Audiooutput {
public:
	~AudiooutputALSA();

	int  open(int8_t bits,uint32_t rate);
	void close(void);
	int  fillBuffer(void *pData,int numSamples);
	int  fillBuffer(mediaframe_t *pFrame);
	void drainBuffer(void);
	int  getBufferState(uint32_t *fill,uint32_t *size);
	int  setVolume(uint8_t vol);
	int  getSamplerate(void);
	int  getBitdepth(void);

private:
	struct alsaoutctx *pCtx;
};
