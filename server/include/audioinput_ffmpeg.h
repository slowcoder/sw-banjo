#pragma once

#include "audioinput.h"

class AudioinputFFMPEG {
public:
	~AudioinputFFMPEG();

	int  openstream(const char *pzURI);
	void close(void);

	mediaframe_t *getNextFrame(void);
private:
	bool bHasNetworkInited;
	struct mediainputffmpegctx *pCtx;
};
