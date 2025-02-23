#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "shared/log.h"
#include "config.h"

#include "audiooutput_alsa.h"

typedef struct alsaoutctx {
  snd_pcm_t *pcm;
  snd_pcm_hw_params_t *hw_params;

  snd_pcm_uframes_t bufferSize;

  int sampleRate,bitDepth;
} alsaoutctx_t;

//#define CFG_OUTPUT_ALSA_PCMDEVICE    "plughw:1,0"
////#define CFG_OUTPUT_ALSA_PCMDEVICE    "hw:0,0"
//#define DEFAULT_DEVICE CFG_OUTPUT_ALSA_PCMDEVICE

AudiooutputALSA::~AudiooutputALSA() {
	LOG("Closing ALSA");
	close();
	this->pCtx = NULL;
}

int  AudiooutputALSA::open(int8_t bits,uint32_t rate) {
	alsaoutctx_t *pCtx;
	snd_pcm_format_t alsabits;
	unsigned int resample = 0;
	unsigned int actualRate;
	unsigned int val;
	int dir;
	int err;

	pCtx = (alsaoutctx_t*)calloc(1,sizeof(alsaoutctx_t));
	if( pCtx == NULL ) return -1;

	pCtx->sampleRate = rate;
	pCtx->bitDepth = bits;

	char pcmdevice[64];
	config_get_string("alsaoutput","pcmdevice",pcmdevice,64);

	err = snd_pcm_open( &pCtx->pcm, pcmdevice, SND_PCM_STREAM_PLAYBACK, 0 );

	// Check for error on open.
	if( err < 0 ) {
		LOGE("Cannot open device \"%s\": %s",pcmdevice,snd_strerror(err));
		goto err_exit;
	} else {
		LOGI("Device opened successfully.");
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

	switch(bits) {
	case 8:
		alsabits = SND_PCM_FORMAT_S8;
		break;
	case 16:
		alsabits = SND_PCM_FORMAT_S16_LE;
		break;
	case 24:
		alsabits = SND_PCM_FORMAT_S24_LE;
		break;
	case 32:
		alsabits = SND_PCM_FORMAT_S32_LE;
		break;
	default:
		LOGW("Unknown bitdepth %u. Defaulting to S16_LE",bits);
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
	actualRate = rate;
	if( (err = snd_pcm_hw_params_set_rate_near(pCtx->pcm, pCtx->hw_params, &actualRate, 0)) < 0 ) {
		LOGE("Cannot set sample rate to %u. (%s)",rate,snd_strerror(err));
		goto err_exit;
	}

	if( actualRate != rate ) {
		LOGW("Sample rate does not match requested rate. (%u requested, %u acquired)",rate,actualRate);
		goto err_exit;
	}

	// Sometimes the default buffer we get is far too big
	val = 500 * 1000; // 500*1000us
	dir = 1;
	if( (err = snd_pcm_hw_params_set_buffer_time_near(pCtx->pcm,pCtx->hw_params,&val,&dir)) < 0 ) {
		LOGE("Cannot set buffer size to %u. (%s)",val,snd_strerror(err));
		goto err_exit;
	}

	// Apply the hardware parameters that we've set.
	if( (err = snd_pcm_hw_params(pCtx->pcm, pCtx->hw_params)) < 0 ) {
		LOGE("Cannot set parameters (%s)",snd_strerror(err));
		goto err_exit;
	} else {
		LOGI("Device parameters have been set successfully.");
	}

	// Get the buffer size.
	snd_pcm_hw_params_get_buffer_size( pCtx->hw_params, &pCtx->bufferSize );
	// If we were going to do more with our sound device we would want to store
	// the buffer size so we know how much data we will need to fill it with.
	LOGI("Buffer size = %lu frames",pCtx->bufferSize);

	// Display the bit size of samples.
	LOGI("Significant bits for linear samples = %u",snd_pcm_hw_params_get_sbits(pCtx->hw_params));

	this->pCtx = pCtx;

  return 0;
err_exit:
	this->close();

	return -1;
}

void AudiooutputALSA::close(void) {

	if( pCtx != NULL ) {
		if( pCtx->hw_params != NULL ) snd_pcm_hw_params_free(pCtx->hw_params);
		if( pCtx->pcm != NULL ) {
			snd_pcm_drain(pCtx->pcm);
			snd_pcm_close(pCtx->pcm);
		}
	}

	free(pCtx);
}

int  AudiooutputALSA::fillBuffer(void *pData,int numSamples) {
	int err;

	if( pCtx == NULL ) {
		LOGE("Told to buffer when context is NULL");
		return -1;
	}

	if( (err = snd_pcm_writei(pCtx->pcm, pData, numSamples)) == -EPIPE ) {
		snd_pcm_prepare(pCtx->pcm);
	}
	if( err < 0 ) {
		if( err == -EPIPE ) {
			LOGW("ALSA underrun");
			return -2;
		} else {
			LOGE("Can't write to PCM device. %s",snd_strerror(err));
			return -3;
		}
	}
	return 0;
}

int  AudiooutputALSA::fillBuffer(mediaframe_t *pFrame) {

	if( pFrame == NULL ) {
		LOGE("Told to play NULL frame");
		return -1;
	}

	int r;
	r = fillBuffer(pFrame->data[0],pFrame->numFrames);

	return r;
}

void AudiooutputALSA::drainBuffer(void) {
//	snd_pcm_drop(pCtx->pcm);
//	snd_pcm_reset(pCtx->pcm);
}


int  AudiooutputALSA::getBufferState(uint32_t *avail,uint32_t *size) {
	snd_pcm_sframes_t bufferAvail;
	snd_pcm_uframes_t bufferSize;

	bufferAvail = snd_pcm_avail(pCtx->pcm);
	snd_pcm_hw_params_get_buffer_size( pCtx->hw_params, &bufferSize );

	LOG(" buffer: %i/%u",bufferAvail,bufferSize);

	if( avail != NULL ) *avail = bufferAvail;
	if( size  != NULL ) *size  = bufferSize;

	return 0;
}

int  AudiooutputALSA::setVolume(uint8_t vol) {
	LOG("");
	return -1;
}

int  AudiooutputALSA::getSamplerate(void) {
	return pCtx->sampleRate;
}

int  AudiooutputALSA::getBitdepth(void) {
	return pCtx->bitDepth;
}
