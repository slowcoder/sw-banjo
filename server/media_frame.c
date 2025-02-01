#include <stdlib.h>
#include <string.h>

#include "media_frame.h"

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
