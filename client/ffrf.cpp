#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shared/log.h"
#include "shared/ffrf-protocol.h"

#include "ffrf.h"

static void handle_ffrf_cmd(ffrpframe_t *pFrame,TCP *pTCP,FILE *in) {
	if( pFrame->hdr.cmd == FFRPCMD_READ ) {
		uint8_t *buff;
		size_t nb;
		int32_t resp;

		buff = (uint8_t*)malloc(pFrame->readcmd.len);
		nb = fread(buff,1,pFrame->readcmd.len,in);
		resp = nb;
		pTCP->send(&resp,sizeof(int32_t));
		if( resp > 0 ) pTCP->send(buff,resp);
		free(buff);
	} else if( pFrame->hdr.cmd == FFRPCMD_SEEK ) {
		int r;
		int32_t rb;

		LOGD("FFRF SEEK:  offset=%i, whence=%u",pFrame->seekcmd.offset,pFrame->seekcmd.whence);

		if(      pFrame->seekcmd.whence == FFRPWHENCE_END ) r = fseek(in,pFrame->seekcmd.offset,SEEK_END);
		else if( pFrame->seekcmd.whence == FFRPWHENCE_SET ) r = fseek(in,pFrame->seekcmd.offset,SEEK_SET);
		else if( pFrame->seekcmd.whence == FFRPWHENCE_CUR ) r = fseek(in,pFrame->seekcmd.offset,SEEK_CUR);
		else {
			ASSERT(0,"STOP");
		}

		rb = r;
		pTCP->send(&rb,sizeof(int32_t));
	} else if( pFrame->hdr.cmd == FFRPSTATUS_PROGRESS ) {
		//LOG("Progress: %lli/%lli",pFrame->progressstatus.pts,pFrame->progressstatus.duration);
		LOG("Progress: %li:%02li / %li:%02li",
			pFrame->progressstatus.pts / 60,
			pFrame->progressstatus.pts % 60,
			pFrame->progressstatus.duration / 60,
			pFrame->progressstatus.duration % 60);

#if 0
		printf("\rProgress: %li:%02li / %li:%02li",
			pFrame->progressstatus.pts / 60,
			pFrame->progressstatus.pts % 60,
			pFrame->progressstatus.duration / 60,
			pFrame->progressstatus.duration % 60);
#endif
	} else if( pFrame->hdr.cmd == FFRPSTATUS_METADATA ) {
		LOG("Got metadata update");
		LOG("  Title : \"%s\"",pFrame->metadata.title);
		LOG("  Album : \"%s\"",pFrame->metadata.album);
		LOG("  Artist: \"%s\"",pFrame->metadata.artist);
	} else {
		ASSERT(0,"Unhandled CMD 0x%x",pFrame->hdr.cmd);
	}

}

static void handle_ffrf(FILE *in,TCP *pTCP) {
	ffrpframe_t frame;
	int r;

	while(1) {

		memset(&frame,0,sizeof(ffrpframe_t));

		r = pTCP->recv(&frame,sizeof(frame.hdr),500);
		if( r < 0 ) { LOGE("TCP error.."); return; }

		if( r == sizeof(frame.hdr) ) {
			LOGD("Got %i (%i)",r,sizeof(frame.hdr));
			r = pTCP->recv((uint8_t*)(&frame) + sizeof(frame.hdr),frame.hdr.payloadlen,500);
			if( r < 0 ) { LOGE("TCP error2.."); return; }
			LOGD("Got %i",r);

			LOGD("Got FFRF cmd");
			LOGD(" cmd=%u",frame.hdr.cmd);
			LOGD(" payloadlen=%u",frame.hdr.payloadlen);

			if( frame.hdr.cmd == FFRPCMD_DONE ) return;

			handle_ffrf_cmd(&frame,pTCP,in);
		}
	}
}

void ffrf_play_file(TCP *pTCP,const char *pzFilename) {
	FILE *in;

	if( pTCP == NULL ) {
		LOGE("Told to play via FFRF over NULL tcp-connection");
		return;
	}

	in = fopen(pzFilename,"rb");
	if( in == NULL ) {
		LOGE("Failed to open file \"%s\"",pzFilename);
		return;
	}

	handle_ffrf(in,pTCP);

	fclose(in);

	LOGD("%p, %s",pTCP,pzFilename);
}
