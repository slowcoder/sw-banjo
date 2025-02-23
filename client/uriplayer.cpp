#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shared/log.h"
#include "shared/uriplayer-protocol.h"

#include "uriplayer.h"

int uriplayer_play_uri(TCP *pTCP,const char *pzURI) {
	uriplayerframe_t *pFrame;

	if( pzURI == NULL ) return -1;

	pFrame = (uriplayerframe_t*)calloc(1,sizeof(pFrame->hdr) + strlen(pzURI));

	pFrame->hdr.cmd = URIPLAYERCMD_PLAYURI;
	pFrame->hdr.payloadlen = strlen(pzURI);

	pTCP->send(pFrame,sizeof(pFrame->hdr));

	pTCP->send((void*)pzURI,pFrame->hdr.payloadlen);

	free(pFrame);

	return 0;
}

int uriplayer_stop(TCP *pTCP) {
	uriplayerframe_t frame;

	frame.hdr.cmd = URIPLAYERCMD_STOP;
	frame.hdr.payloadlen = 0;

	pTCP->send(&frame,sizeof(frame.hdr) + frame.hdr.payloadlen);

	return 0;
}
