#include <stdlib.h>
#include <string.h>
#include <soxr.h>
#include "shared/log.h"
//#include "shared/config.h"

#include "mediaresampler.h"

#define CFG_OUTPUT_SAMPLERATE 48000
#define CFG_OUTPUT_BITSPERSAMPLE 16

typedef struct mediaresamplerctx {
	soxr_t soxr;

	uint32_t targetRate;
	uint8_t targetBits;

	uint32_t prev_inputRate;
	eMediasamplefmt prev_inputFormat;

	int is_initialized;
} mediaresamplerctx_t;

int Mediaresampler::initialize(mediaframe_t *pFrame) {
	double irate,orate;
	soxr_error_t error;
	static soxr_io_spec_t iospec = {};

	mediaresamplerctx_t *pMe = this->pCtx;

	irate = pFrame->rate;
	orate = pMe->targetRate;

	switch(pFrame->fmt) {
	case eMediasamplefmt_S16: iospec.itype = SOXR_INT16_I; break;
	case eMediasamplefmt_S32: iospec.itype = SOXR_INT32_I; break;
	case eMediasamplefmt_F32: iospec.itype = SOXR_FLOAT32_I; break;
	default:
		LOGE("Unsupported sample format");
		break;
	}

	if( pMe->targetBits == 16 ) {
		iospec.otype = SOXR_INT16_I;
	} else if( pMe->targetBits == 32 ) {
		iospec.otype = SOXR_INT32_I;		
	} else {
		LOGE("Unsupported bit-depth");
		return -1;
	}
	iospec.scale = 1.0; // "gain"

	soxr_quality_spec_t quality;

	quality = soxr_quality_spec(SOXR_VHQ, 0);

	pMe->soxr = soxr_create(
      irate, orate, 2,             /* Input rate, output rate, # of channels. */
      &error,                         /* To report any error during creation. */
      &iospec, // IO spec
      &quality, // Quality
      NULL); // Runtime

	if( error ) {
		LOGE("Failed to create SoX instance");
		return -1;
	}

	LOGI("Initialized. %i -> %u",pFrame->rate,pMe->targetRate);

	pMe->prev_inputRate = pFrame->rate;
	pMe->prev_inputFormat = pFrame->fmt;

	pMe->is_initialized = 1;
	return 0;
}


int Mediaresampler::open(int targetBitdepth,int targetRate) {

	pCtx = (mediaresamplerctx_t*)calloc(1,sizeof(mediaresamplerctx_t));
	if( pCtx == NULL ) return -1;

	pCtx->targetRate = targetRate;
	pCtx->targetBits = targetBitdepth;

	return 0;
}

void Mediaresampler::close(void) {
	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	memset(pCtx,0,sizeof(mediaresamplerctx_t));
	free(pCtx);
}

mediaframe_t *Mediaresampler::process(mediaframe_t *pInFrame) {
	soxr_error_t error;
	size_t odone;

	if( pCtx == NULL ) {
		LOGE("Told to process when context is NULL");
		return NULL;
	}
	if( pInFrame == NULL ) {
		LOGE("Told to process NULL frame");
		return NULL;
	}
	if( pInFrame->fmt == eMediasamplefmt_Invalid ) {
		LOGE("Told to process invalid sampleformat");
		return NULL;
	}

	// If our input config has changed, tear down SoXR
	// and re-init
	if( (pCtx->prev_inputRate != pInFrame->rate) ||
		(pCtx->prev_inputFormat != pInFrame->fmt) ) {

		if( pCtx->soxr != NULL ) {
			soxr_delete(pCtx->soxr);
		}
		initialize(pInFrame);
	}

	mediaframe_t *pNewframe;
	pNewframe = mediaframe_alloc();

	pNewframe->numFrames = ((pInFrame->numFrames * pCtx->targetRate)+0.5) / pInFrame->rate; 
	pNewframe->data[0] = malloc(pNewframe->numFrames * 2 * sizeof(int16_t) * 2);
	pNewframe->numChannels = 2;
	pNewframe->bIsPlanar = 0;

	if( pCtx->targetBits == 16 ) pNewframe->fmt = eMediasamplefmt_S16;
	else if( pCtx->targetBits == 32 ) pNewframe->fmt = eMediasamplefmt_S32;
	else {
		LOGE("Invalid targetBits");
		mediaframe_free(pNewframe);
		return NULL;
	}

	error = soxr_process(pCtx->soxr,
		pInFrame->data[0], // Input buffer
		pInFrame->numFrames, // Input length (in samples per channel)
		NULL, // *samples used
		pNewframe->data[0], // Output buffer
		pNewframe->numFrames, // Output length (in samples per channel)
		&odone); // *samples outputted

	if( error ) {
		LOGE("SoXR error: %s",error);
	}

	return pNewframe;
}
