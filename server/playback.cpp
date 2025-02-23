#include "shared/log.h"
#include "shared/util.h"

#include "audioinput_ffmpeg.h"
#include "audioinput_ffrf.h"
#include "audiooutput_alsa.h"
#include "mediaresampler.h"
#include "config.h"

#include "playback.h"

void Playback::playbackthread(void) {

}

Playback::Playback() {
	int bitdepth,samplerate;

	config_get_int("alsaoutput","bitdepth",&bitdepth);
	config_get_int("alsaoutput","samplerate",&samplerate);

	pOutput = new AudiooutputALSA();
	if( pOutput->open(bitdepth,samplerate) != 0 ) {
		LOGE("Failed to open output device");
		delete pOutput;
		pOutput = NULL;
	}
}

Playback::~Playback() {
	delete pOutput;
	pOutput = NULL;
}

int Playback::play_uri(const char *pzURI) {
	AudioinputFFMPEG   *pInput;
	Mediaresampler   *pResampler;
	mediaframe_t *pDecoded,*pResampled;

	LOG("Fooooooooooooooooooo");

	if( pOutput == NULL ) {
		LOGE("Told to play with NULL output device");
		return -1;
	}

	pInput = new AudioinputFFMPEG();
	pInput->openstream(pzURI);

	pResampler = new Mediaresampler();
	pResampler->open(pOutput->getBitdepth(),pOutput->getSamplerate());

	do {
		pDecoded = pInput->getNextFrame();

		pResampled = pResampler->process(pDecoded);

		pOutput->fillBuffer(pResampled);

		mediaframe_free(pDecoded);
		mediaframe_free(pResampled);
	} while(pDecoded != NULL);

	delete pResampler;
	delete pInput;

	pOutput->drainBuffer();

	pResampler = NULL;
	pInput = NULL;

	LOG("Playback ended");
	return 0;	
}

int Playback::play_file(const char *pzFilename) {
	return -1;
}

void Playback::stop(void) {

}

int Playback::play_ffrf(TCP *pTCP) {
	AudioinputFFRF   *pInput;
	Mediaresampler   *pResampler;
	mediaframe_t *pDecoded,*pResampled;

	if( pOutput == NULL ) {
		LOGE("Told to play with NULL output device");
		return -1;
	}

	pInput = new AudioinputFFRF();
	pInput->handleConnection(pTCP);

	pResampler = new Mediaresampler();
	pResampler->open(pOutput->getBitdepth(),pOutput->getSamplerate());

	do {
		pDecoded = pInput->getNextFrame();

		pResampled = pResampler->process(pDecoded);

		pOutput->fillBuffer(pResampled);
		pInput->updateMeta(pDecoded);

		mediaframe_free(pDecoded);
		mediaframe_free(pResampled);
	} while(pDecoded != NULL);

	delete pResampler;
	delete pInput;

	pOutput->drainBuffer();

	pResampler = NULL;
	pInput = NULL;

	LOG("Playback ended");
	return 0;	
}


void playback_test(void) {
	AudioinputFFMPEG *pInput;
	AudiooutputALSA  *pOutput;
	Mediaresampler   *pResampler;
	mediaframe_t *pDecoded,*pResampled;


	pInput = new AudioinputFFMPEG();
//	pInput->openstream("http://http-live.sr.se/p4malmo-aac-192");
	//pInput->openstream("file:///home/jajac/Downloads/Hip Hop Remix Mix.mp3");
//	pInput->openstream("file:///home/jajac/src/audio/banjo3/test.flac");
	// ^-- Fix this.. Probably plays wrong streamndx
	pInput->openstream("file:///home/jajac/src/audio/banjo3/test.mp3");

	pOutput = new AudiooutputALSA();
	pOutput->open(32,48000);

	pResampler = new Mediaresampler();
	pResampler->open(32,pOutput->getSamplerate());

	do {
		pDecoded = pInput->getNextFrame();

		//LOG("pDecoded=%p",pDecoded);

		pResampled = pResampler->process(pDecoded);

		pOutput->fillBuffer(pResampled);

		mediaframe_free(pDecoded);
		mediaframe_free(pResampled);
	} while(pDecoded != NULL);
}
