#pragma once

#include "shared/tcp.h"
#include "shared/util.h"

#include "mediaframe.h"

#define METACACHE_MAXSTR 64

class AudioinputFFRF {
public:
	AudioinputFFRF();
	~AudioinputFFRF();

	int openPort(uint16_t port);
	int handleConnection(TCP *pTCP);
	void close(void);

	int updateMeta(mediaframe_t *pFrame);

	mediaframe_t *getNextFrame(void);
private:
//	TCP *pTCP;

	int tryGetNextFrame(mediaframe_t **ppRetFrame);
	int handleUpdatedMetadata(void);
	void sendMetadataUpdate(void);

	struct {
		int64_t duration;
		int64_t pts;
		char title[METACACHE_MAXSTR];
		char album[METACACHE_MAXSTR];
		char artist[METACACHE_MAXSTR];
	} metacache;

	struct ffrfctx *pCtx;
};
