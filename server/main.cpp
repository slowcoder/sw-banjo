#include <stdlib.h>
#include <unistd.h>

#include "shared/bdp/bdp.h"
#include "shared/tcp.h"
#include "audiooutput_alsa.h"
#include "audioinput_ffrf.h"
#include "playback.h"
#include "shared/uriplayer-protocol.h"

#include "shared/log.h"

static void handle_uriplayer(Playback *pPB,TCP *pS) {
	uriplayerframe_t frame;
	char *pzURI;

	pS->recv(&frame,sizeof(frame.hdr),500);

	if( frame.hdr.cmd == URIPLAYERCMD_PLAYURI ) {
		pzURI = (char*)calloc(1,frame.hdr.payloadlen+1);
		pS->recv(pzURI,frame.hdr.payloadlen,500);
		LOG("URIPlayer PLAYURI");
		LOG(" URI: \"%s\"",pzURI);
	} else {
		LOGE("Unhandled URIPlayer command 0x%x",frame.hdr.cmd);
	}

	pPB->play_uri(pzURI);

	free(pzURI);
}

void playback_test(void);

int main(void) {
	BDP *pBDP;

#if 0
	AudioinputFFRF *pRF = new AudioinputFFRF();

	pRF->openPort(8183);

	for(int i=0;i<100;i++) {
		pRF->getNextFrame();
	}
#endif

#if 0
	playback_test();
	return 0;
#endif

#if 0
	AudiooutputALSA *pA = new AudiooutputALSA();
	pA->open(32,48000);
	pA->getBufferState(NULL,NULL);
	pA->close();
	delete pA;
#endif

	pBDP = BDP::getInstance(1);
	pBDP->advertiseService(BDP::URIPlayer, 8181);
	pBDP->advertiseService(BDP::SPSink, 8182);
	pBDP->advertiseService(BDP::FFRF, 8183);

	TCP *pURI  = new TCP();
	TCP *pFFRF = new TCP();

	pURI->listen(8181);
	pFFRF->listen(8183);

	Playback *pPB = new Playback();

	while(1) {
#if 1
		if( pURI->accept() == 0 ) {
			LOG("Got a new URI connection");
			handle_uriplayer(pPB,pURI);
			pURI->close();
		}
#endif
		if( pFFRF->accept() == 0 ) {
			LOG("Got a new FFRF connection");
			//handle_ffrpsession(pFFRF);
			pPB->play_ffrf(pFFRF);
			LOG("Closing FFRF connection");
			pFFRF->close();
		}

		usleep(100 * 1000);
	}
	delete pPB;

	return 0;
}
