#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "shared/config.h"
#include "log.h"

#include "mediainput_alsa.h"

//#define CFG_INPUT_ALSA_PCMDEVICE    "hw:1,0"
//#define CFG_INPUT_ALSA_SAMPLERATE    48000
//#define CFG_INPUT_ALSA_BITSPERSAMPLE 16

#define FORMAT_DEFAULT SND_PCM_FORMAT_S16LE

typedef struct mediainput {
	snd_pcm_t *handle;
	snd_output_t *log;

	snd_pcm_hw_params_t *hw_params;
} mediainput_t;

static int open_input(mediainput_t *pCtx) {
	snd_pcm_info_t *info;
	int err;
	
	snd_pcm_info_alloca(&info);

#if 0
	err = snd_output_stdio_attach(&pCtx->log, stderr, 0);
	assert(err >= 0);
#endif

	err = snd_pcm_open(&pCtx->handle, CFG_INPUT_ALSA_PCMDEVICE, SND_PCM_STREAM_CAPTURE, 0);
	if( err < 0 ) {
		LOGE("audio open error: %s", snd_strerror(err));
		return -1;
	}

#if 0
	if( (err = snd_pcm_info(pCtx->handle, info)) < 0 ) {
		LOGE("info error: %s", snd_strerror(err));
		return -2;
	}
#endif
	return 0;
}

static int set_params(mediainput_t *pCtx) {
	int err;

	// Allocate the hardware parameter structure.
	if( (err = snd_pcm_hw_params_malloc(&pCtx->hw_params)) < 0 ) {
		LOGE("Cannot allocate hardware parameter structure (%s)",snd_strerror(err));
		goto err_exit;
	}

	if( (err = snd_pcm_hw_params_any(pCtx->handle, pCtx->hw_params)) < 0 ) {
		LOGE("Cannot initialize hardware parameter structure (%s)",snd_strerror(err));
		goto err_exit;
	}
#if 1
	// Disable resampling.
	unsigned int resample = 0;
	err = snd_pcm_hw_params_set_rate_resample(pCtx->handle, pCtx->hw_params, resample);
	if( err < 0 ) {
		LOGE("Resampling setup failed for playback: %s",snd_strerror(err));
		goto err_exit;
	}

	// Set access to RW interleaved.
	if( (err = snd_pcm_hw_params_set_access(pCtx->handle, pCtx->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ) {
		LOGE("Cannot set access type (%s)",snd_strerror(err));
		goto err_exit;
	}

	snd_pcm_format_t alsabits;
	alsabits = SND_PCM_FORMAT_S16_LE;

	// Test the format (bits)
	if( (err = snd_pcm_hw_params_test_format(pCtx->handle,pCtx->hw_params,alsabits)) < 0 ) {
		LOGE("Device doesn't support selected bit-depth");
		goto err_exit;
	}

	// Set the format (bits)
	if( (err = snd_pcm_hw_params_set_format(pCtx->handle,pCtx->hw_params,alsabits)) < 0 ) {
		LOGE("Cannot set sample format (%s)",snd_strerror(err));
		goto err_exit;
	}

	// Set channels to stereo (2).
	if( (err = snd_pcm_hw_params_set_channels(pCtx->handle, pCtx->hw_params, 2)) < 0 ) {
		LOGE("Cannot set channel count (%s)",snd_strerror(err));
		goto err_exit;
	}

	// Set sample rate.
	unsigned int actualRate = CFG_INPUT_ALSA_SAMPLERATE;
	if( (err = snd_pcm_hw_params_set_rate_near(pCtx->handle, pCtx->hw_params, &actualRate, 0)) < 0 ) {
		LOGE("Cannot set sample rate to %u. (%s)",CFG_OUTPUT_SAMPLERATE,snd_strerror(err));
		goto err_exit;
	}

	if( actualRate != CFG_INPUT_ALSA_SAMPLERATE ) {
		LOGW("Sample rate does not match requested rate. (%u requested, %u acquired)",CFG_INPUT_ALSA_SAMPLERATE,actualRate);
		goto err_exit;
	}
#endif
	if( (err = snd_pcm_hw_params(pCtx->handle, pCtx->hw_params)) < 0 ) {
		LOGE("cannot set parameters (%s)",snd_strerror (err));
		goto err_exit;
	}

	snd_pcm_hw_params_free(pCtx->hw_params);

	if( (err = snd_pcm_prepare(pCtx->handle)) < 0 ) {
		LOGE("cannot prepare audio interface for use (%s)",snd_strerror(err));
		goto err_exit;
	}

	LOG("ALSA Input opened at %u/%u",16,CFG_INPUT_ALSA_SAMPLERATE);

	return 0;

err_exit:
	return -1;
}

static void dump_status(struct mediainput *pCtx) {
	snd_pcm_status_t *status;
	int res;

	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(pCtx->handle, status))<0) {
		LOGE("status error: %s", snd_strerror(res));
		return;
	}

	LOGE("Status(R/W):");
	snd_pcm_status_dump(status, pCtx->log);
}

struct mediainput *mediainput_alsa_openstream(void) {
	mediainput_t *pCtx = NULL;

	pCtx = (mediainput_t*)calloc(1,sizeof(mediainput_t));

	if( open_input(pCtx) != 0 ) goto err_exit;

	if( set_params(pCtx) != 0 ) goto err_exit;

	return pCtx;
err_exit:
	if( pCtx == NULL ) return NULL;

	mediainput_alsa_close(pCtx);

	return NULL;
}

void               mediainput_alsa_close(struct mediainput *pCtx) {
	if( pCtx == NULL ) return;

	snd_pcm_close(pCtx->handle);

	memset(pCtx,0,sizeof(mediainput_t));
	free(pCtx);
}

mediaframe_t      *mediainput_alsa_getnextframe(struct mediainput *pCtx) {
	mediaframe_t *pFrame;

	pFrame = mediaframe_alloc();
	pFrame->channels = 2;
	pFrame->rate = CFG_INPUT_ALSA_SAMPLERATE;
	pFrame->is_planar = 0;
	pFrame->fmt = eMediasamplefmt_S16;

	// We're trying for 1/10th of a second of data
	pFrame->data[0] = malloc((CFG_INPUT_ALSA_SAMPLERATE * 2 * 2) / 10);

	snd_pcm_sframes_t sf;
	sf = snd_pcm_readi(pCtx->handle,pFrame->data[0],CFG_INPUT_ALSA_SAMPLERATE/10);
	if( sf < 0 ) {
		if( sf == -EPIPE ) {
			snd_pcm_prepare(pCtx->handle);
			goto err_exit;
		}
		LOGE("Unable to read frames (%s)",snd_strerror(sf));
		dump_status(pCtx);
		goto err_exit;
	}

	pFrame->nb_samples = sf;

	return pFrame;
err_exit:
	mediaframe_free(pFrame);
	return NULL;
}

