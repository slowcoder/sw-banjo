#include <stdlib.h>
#include <string.h>
#include <soxr.h>
#include "log.h"
#include "shared/config.h"

#include "mediaresampler.h"

typedef struct mediaresampler {
	soxr_t soxr;

	uint32_t targetRate;
	uint8_t targetBits;

	uint32_t prev_inputRate;
	eMediasamplefmt prev_inputFormat;

	int is_initialized;
} mediaresampler_t;

static int initialize(mediaresampler_t *pMe,mediaframe_t *pFrame) {
	double irate,orate;
	soxr_error_t error;
	static soxr_io_spec_t iospec = {0};

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

	pMe->soxr = soxr_create(
      irate, orate, 2,             /* Input rate, output rate, # of channels. */
      &error,                         /* To report any error during creation. */
      &iospec, // IO spec
      NULL, // Quality
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


struct mediaresampler *mediaresampler_open(void) {
	mediaresampler_t *pCtx;

	pCtx = (mediaresampler_t*)calloc(1,sizeof(mediaresampler_t));

	pCtx->targetRate = CFG_OUTPUT_SAMPLERATE;
	pCtx->targetBits = CFG_OUTPUT_BITSPERSAMPLE;

	return pCtx;
}

void                   mediaresampler_close(struct mediaresampler *pCtx) {
	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	memset(pCtx,0,sizeof(mediaresampler_t));
	free(pCtx);
}

mediaframe_t 		  *mediaresampler_process(struct mediaresampler *pCtx,mediaframe_t *pInFrame) {
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
		initialize(pCtx,pInFrame);
	}

	mediaframe_t *pNewframe;
	pNewframe = mediaframe_alloc();

	pNewframe->nb_samples = ((pInFrame->nb_samples * pCtx->targetRate)+0.5) / pInFrame->rate; 
	pNewframe->data[0] = malloc(pNewframe->nb_samples * 2 * sizeof(int16_t) * 2);
	pNewframe->channels = 2;
	pNewframe->is_planar = 0;

	if( pCtx->targetBits == 16 ) pNewframe->fmt = eMediasamplefmt_S16;
	else if( pCtx->targetBits == 32 ) pNewframe->fmt = eMediasamplefmt_S32;
	else {
		LOGE("Invalid targetBits");
		mediaframe_free(pNewframe);
		return NULL;
	}

	error = soxr_process(pCtx->soxr,
		pInFrame->data[0], // Input buffer
		pInFrame->nb_samples, // Input length (in samples per channel)
		NULL, // *samples used
		pNewframe->data[0], // Output buffer
		pNewframe->nb_samples, // Output length (in samples per channel)
		&odone); // *samples outputted

	if( error ) {
		LOGE("SoXR error: %s",error);
	}

	return pNewframe;
}
