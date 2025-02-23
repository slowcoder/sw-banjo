#pragma once

#include "audiooutput.h"
#include "audiooutput_alsa.h"

#include "shared/tcp.h"

class Playback {
public:
	Playback();
	~Playback();

	int play_uri(const char *pzURI);
	int play_file(const char *pzFilename);
	int play_ffrf(TCP *pTCP);

	void stop(void);
private:
	void playbackthread(void);

//	Audiooutput *pOutput;
	AudiooutputALSA *pOutput;
};
