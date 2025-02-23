#pragma once

#include "mediaframe.h"

class Mediaresampler {
public:
	int  open(int targetBitdepth,int targetRate);
	void close(void);

	mediaframe_t *process(mediaframe_t *pFrame);

private:
	int initialize(mediaframe_t *pFrame);

	struct mediaresamplerctx *pCtx;
};
