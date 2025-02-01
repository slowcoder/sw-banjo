#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "ipc.h"
#include "mediainput_avcodec.h"
#include "mediaresampler.h"
#include "mediaoutput_alsa.h"
#include "shared/config.h"

#define SAMPLERATE    CFG_OUTPUT_SAMPLERATE
#define BITSPERSAMPLE CFG_OUTPUT_BITSPERSAMPLE

typedef struct playback {
	pthread_t thread_hdl;

	struct mediainput     *pMediaInput;
	struct mediaresampler *pMediaResampler;
	struct mediaoutput    *pMediaOutput;
} playback_t;

static void playstream(playback_t *pCtx,char *pzURI) {

	if( pCtx->pMediaInput != NULL ) {
		mediainput_avcodec_close(pCtx->pMediaInput);
		pCtx->pMediaInput = NULL;
	}
	if( pCtx->pMediaResampler != NULL ) {
		mediaresampler_close(pCtx->pMediaResampler);
		pCtx->pMediaResampler = NULL;
	}

	// No need to close/open the mediaoutput, as we're always running the same bit/samplerrate
	if( pCtx->pMediaOutput != NULL ) {
		mediaoutput_alsa_flush(pCtx->pMediaOutput);
	} else {
		pCtx->pMediaOutput = mediaoutput_alsa_open();
		mediaoutput_alsa_setplaybackvolume(50);
	}

	pCtx->pMediaInput = mediainput_avcodec_openstream(pzURI);
	pCtx->pMediaResampler = mediaresampler_open();
}

static void stopstream(playback_t *pCtx) {
	if( pCtx->pMediaInput != NULL ) {
		mediainput_avcodec_close(pCtx->pMediaInput);
		pCtx->pMediaInput = NULL;
	}
	if( pCtx->pMediaResampler != NULL ) {
		mediaresampler_close(pCtx->pMediaResampler);
		pCtx->pMediaResampler = NULL;
	}
}

static void *playback_thread(void *pArg) {
	playback_t *pCtx = (playback_t*)pArg;
	IPCEvent_t ev;
	int bIsPlaying = 0;

	while(1) {
		while( IPCEvent_Poll(&ev) == 0 ) {

			switch(ev.type) {
			case eEventType_Playstream:
				printf("Told to play a stream (%s)\n",ev.playstream.pzURI);
				playstream(pCtx,ev.playstream.pzURI);
				bIsPlaying = 1;
				break;
			case eEventType_Playback_Stop:
				stopstream(pCtx);
				bIsPlaying = 0;
				break;
			case eEventType_Volume_Set:
				mediaoutput_alsa_setplaybackvolume(ev.volume_set.level);
				break;
			default:
				fprintf(stderr,"Got unhandled IPC event of type 0x%x\n",ev.type);
			}
		}

		if( bIsPlaying ) {
			mediaframe_t *pFrame = NULL;
			mediaframe_t *pResampledFrame = NULL;

			pFrame = mediainput_avcodec_getnextframe(pCtx->pMediaInput);
			pResampledFrame = mediaresampler_process(pCtx->pMediaResampler,pFrame);
			mediaframe_free(pFrame);
			mediaoutput_alsa_buffer(pCtx->pMediaOutput,pResampledFrame);
			mediaframe_free(pResampledFrame);
		} else {
			usleep(100*1000);
		}
	}

	return NULL;
}

struct playback *playback_init(void) {
	playback_t *pCtx = NULL;
	int r;

	pCtx = (playback_t*)calloc(1,sizeof(playback_t));
	if( pCtx == NULL ) return NULL;

	r = pthread_create(&pCtx->thread_hdl,NULL,playback_thread,pCtx);
	if( r != 0 ) {
		free(pCtx);
		return NULL;
	}

	return pCtx;
}

void             playback_deinit(struct playback *pCtx) {
	if( pCtx == NULL ) return;

	memset(pCtx,0,sizeof(playback_t));
	free(pCtx);
}

int              playback_set_volume(struct playback *pCtx,uint8_t level) {
	IPCEvent_t ev;

	printf("PB: Should set volume to %u\n",level);

	ev.type = eEventType_Volume_Set;
	ev.volume_set.level = level;
	IPCEvent_Post(&ev);

	return 0;
}

int              playback_play_stream(struct playback *pCtx,char *pzURI) {
	IPCEvent_t ev;

	printf("PB: Should play stream \"%s\"\n",pzURI);

	ev.type = eEventType_Playstream;
	strncpy(ev.playstream.pzURI,pzURI,1024);
	IPCEvent_Post(&ev);

	return 0;
}

int              playback_stop(struct playback *pCtx) {
	IPCEvent_t ev;

	ev.type = eEventType_Playback_Stop;
	IPCEvent_Post(&ev);

	return 0;
}