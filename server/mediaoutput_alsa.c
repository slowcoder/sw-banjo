#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "shared/config.h"
#include "log.h"

#include "mediaoutput_alsa.h"

#define DEFAULT_DEVICE CFG_ALSA_PCMDEVICE

typedef struct mediaoutput {
  snd_pcm_t *pcm;
  snd_pcm_hw_params_t *hw_params;

  snd_pcm_uframes_t bufferSize;
} mediaoutput_t;

struct mediaoutput *mediaoutput_alsa_open(void) {
	mediaoutput_t *pCtx;
	int err;

	pCtx = (mediaoutput_t*)calloc(1,sizeof(mediaoutput_t));
	if( pCtx == NULL ) return NULL;

	err = snd_pcm_open( &pCtx->pcm, DEFAULT_DEVICE, SND_PCM_STREAM_PLAYBACK, 0 );

	// Check for error on open.
	if( err < 0 ) {
		LOGE("cannot open audio device \"%s\": %s",DEFAULT_DEVICE,snd_strerror(err));
		goto err_exit;
	} else {
		LOGI("Audio device opened successfully.");
	}

	// Allocate the hardware parameter structure.
	if( (err = snd_pcm_hw_params_malloc(&pCtx->hw_params)) < 0 ) {
		LOGE("Cannot allocate hardware parameter structure (%s)",snd_strerror(err));
		goto err_exit;
	}

	if( (err = snd_pcm_hw_params_any(pCtx->pcm, pCtx->hw_params)) < 0 ) {
		LOGE("Cannot initialize hardware parameter structure (%s)",snd_strerror(err));
		goto err_exit;
	}

	// Disable resampling.
	unsigned int resample = 0;
	err = snd_pcm_hw_params_set_rate_resample(pCtx->pcm, pCtx->hw_params, resample);
	if( err < 0 ) {
		LOGE("Resampling setup failed for playback: %s",snd_strerror(err));
		goto err_exit;
	}

	// Set access to RW interleaved.
	if( (err = snd_pcm_hw_params_set_access(pCtx->pcm, pCtx->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ) {
		LOGE("Cannot set access type (%s)",snd_strerror(err));
		goto err_exit;
	}

	snd_pcm_format_t alsabits;
	switch(CFG_OUTPUT_BITSPERSAMPLE) {
//	case 8:
//		alsabits = SND_PCM_FORMAT_S8;
//		break;
//	case 24:
//		alsabits = SND_PCM_FORMAT_S24_LE;
//		break;
	case 32:
		alsabits = SND_PCM_FORMAT_S32_LE;
		break;
	default:
		LOGW("Unknown bitdepth %u. Defaulting to S16_LE",CFG_OUTPUT_BITSPERSAMPLE);
	case 16:
		alsabits = SND_PCM_FORMAT_S16_LE;
		break;
	}

	// Test the format (bits)
	if( (err = snd_pcm_hw_params_test_format(pCtx->pcm,pCtx->hw_params,alsabits)) < 0 ) {
		LOGE("Device doesn't support selected bit-depth");
		goto err_exit;
	}

	// Set the format (bits)
	if( (err = snd_pcm_hw_params_set_format(pCtx->pcm,pCtx->hw_params,alsabits)) < 0 ) {
		LOGE("Cannot set sample format (%s)",snd_strerror(err));
		goto err_exit;
	}

	// Set channels to stereo (2).
	if( (err = snd_pcm_hw_params_set_channels(pCtx->pcm, pCtx->hw_params, 2)) < 0 ) {
		LOGE("Cannot set channel count (%s)",snd_strerror(err));
		goto err_exit;
	}

	// Set sample rate.
	unsigned int actualRate = CFG_OUTPUT_SAMPLERATE;
	if( (err = snd_pcm_hw_params_set_rate_near(pCtx->pcm, pCtx->hw_params, &actualRate, 0)) < 0 ) {
		LOGE("Cannot set sample rate to %u. (%s)",CFG_OUTPUT_SAMPLERATE,snd_strerror(err));
		goto err_exit;
	}

	if( actualRate != CFG_OUTPUT_SAMPLERATE ) {
		LOGW("Sample rate does not match requested rate. (%u requested, %u acquired)",CFG_OUTPUT_SAMPLERATE,actualRate);
	}

	// Sometimes the default buffer we get is far too big
	unsigned int val = 500 * 1000; // 500*1000us
	int dir = 1;
	if( (err = snd_pcm_hw_params_set_buffer_time_near(pCtx->pcm,pCtx->hw_params,&val,&dir)) < 0 ) {
		LOGE("Cannot set buffer size to %u. (%s)",val,snd_strerror(err));
		goto err_exit;
	}

	// Apply the hardware parameters that we've set.
	if( (err = snd_pcm_hw_params(pCtx->pcm, pCtx->hw_params)) < 0 ) {
		LOGE("Cannot set parameters (%s)",snd_strerror(err));
		goto err_exit;
	} else {
		LOGI("Audio device parameters have been set successfully.");
	}

	// Get the buffer size.
	snd_pcm_hw_params_get_buffer_size( pCtx->hw_params, &pCtx->bufferSize );
	// If we were going to do more with our sound device we would want to store
	// the buffer size so we know how much data we will need to fill it with.
	LOGI("Buffer size = %lu frames",pCtx->bufferSize);

	// Display the bit size of samples.
	LOGI("Significant bits for linear samples = %u",snd_pcm_hw_params_get_sbits(pCtx->hw_params));

  return pCtx;
err_exit:
	if( pCtx != NULL ) mediaoutput_alsa_close(pCtx);
	return NULL;
}

void                mediaoutput_alsa_close(struct mediaoutput *pCtx) {

	if( pCtx == NULL ) {
		LOGE("Told to close when context is NULL");
		return;
	}

	if( pCtx->hw_params != NULL ) snd_pcm_hw_params_free(pCtx->hw_params);
	if( pCtx->pcm != NULL ) {
		snd_pcm_drain(pCtx->pcm);
		snd_pcm_close(pCtx->pcm);
	}

	memset(pCtx,0,sizeof(mediaoutput_t));
	free(pCtx);
}

int 				mediaoutput_alsa_buffer(struct mediaoutput *pCtx,mediaframe_t *pFrame) {
	int err;

	if( pCtx == NULL ) {
		LOGE("Told to buffer when context is NULL");
		return -1;
	}


	if( (err = snd_pcm_writei(pCtx->pcm, pFrame->data[0], pFrame->nb_samples)) == -EPIPE ) {
		snd_pcm_prepare(pCtx->pcm);
	}
	if( err < 0 ) {
		if( err == -EPIPE ) {
			LOGW("ALSA underrun");
		} else {
			LOGE("Can't write to PCM device. %s\n",snd_strerror(err));
		}
	}

	return 0;
}

void                mediaoutput_alsa_flush(struct mediaoutput *pCtx) {
	LOGE("Implement me");
	return;
}

void                mediaoutput_alsa_setplaybackvolume(uint8_t volume) {
	int err;
	long min, max;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	const char *card = CFG_ALSA_MIXERDEVICE;
//	const char *selem_name = "Master";
	const char *selem_name = CFG_ALSA_MIXERELEMENT;

//	snd_mixer_open(&handle, 0);
	if( (err = snd_mixer_open(&handle, 0)) < 0 ) {
		LOGE("Cannot open mixer (%s)",snd_strerror(err));
	}

	//snd_mixer_attach(handle, card);
	if( (err = snd_mixer_attach(handle, card)) < 0 ) {
		LOGE("Cannot attach mixer (%s)",snd_strerror(err));
	}

	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

	snd_mixer_close(handle);
}