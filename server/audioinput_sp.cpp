
#include "audioinput_sp.h"

AudioinputSP::AudioinputSP() {

	pTCP = new TCP();
}

AudioinputSP::~AudioinputSP() {
	delete pTCP;
}

int AudioinputSP::openPort(uint16_t port) {
	int r;

	r = pTCP->listen(port);

	return r;
}

mediaframe_t *AudioinputSP::getNextFrame(void) {
	return NULL;
}
