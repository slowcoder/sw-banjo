#include <stdlib.h>
#include <string.h>

#include "shared/log.h"

#include "mediaframe.h"

mediaframe_t *mediaframe_alloc(void) {
	mediaframe_t *pRet;

	pRet = (mediaframe_t*)calloc(1,sizeof(mediaframe_t));

	return pRet;
}

void          mediaframe_free(mediaframe_t *pFrame) {
	int i;

	if( pFrame == NULL ) return;

	for(i=0;i<MEDIAFRAME_MAX_PLANES;i++) {
		if( pFrame->data[i] != NULL ) {
			free(pFrame->data[i]);
		}
	}

	memset(pFrame,0,sizeof(mediaframe_t));
	free(pFrame);
}

int           mediaframe_getsamplesize(mediaframe_t *pFrame) {

	switch(pFrame->fmt) {
	case eMediasamplefmt_S16: return sizeof(int16_t);
	case eMediasamplefmt_S32: return sizeof(int32_t);
	case eMediasamplefmt_F32: return sizeof(float);
	default:
		LOGE("Unsupported sample format %i",pFrame->fmt);
	}
	return -1;
}