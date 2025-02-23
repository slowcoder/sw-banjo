#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <thread>

#include "shared/log.h"
#include "shared/bdp/bdp.h"
#include "shared/tcp.h"
#include "shared/ffrf-protocol.h"
#include "ffrf.h"
#include "uriplayer.h"

static BDP *pBDP = NULL;

BDP::Service *getFirstPeer(BDP::eServices desiredService);
TCP *connectToFirstServicing(BDP::eServices desiredService);

static void uriplayer_play(char *pzURI) {
	TCP *pTCP;

	pTCP = connectToFirstServicing(BDP::URIPlayer);

	if( pTCP == NULL ) {
		LOGE("Couldn't find a device servicing URIPlayer");
		return;
	}

	uriplayer_play_uri(pTCP,pzURI);
}

static void uriplayer_stop(void) {
	TCP *pTCP;

	pTCP = connectToFirstServicing(BDP::URIPlayer);

	if( pTCP == NULL ) {
		LOGE("Couldn't find a device servicing URIPlayer");
		return;
	}

	uriplayer_stop(pTCP);
}

int main(int argc,char **argv) {

	pBDP = BDP::getInstance(0);

	if( argc < 2 ) {
		fprintf(stderr,"Usage: %s cmd [arg]\n",argv[0]);
		fprintf(stderr," Available commands:\n");
		fprintf(stderr,"   playuri [uri to stream]\n");
		fprintf(stderr,"   playfile FILENAME\n");
		fprintf(stderr,"   stop\n");
		return 0;
	}

	if( strcmp(argv[1],"playuri") == 0 ) {
		if( argc > 2 ) {
			uriplayer_play(argv[2]);
		} else {
			printf("Playing default stream\n");
			//play_stream("http://http-live.sr.se/p3-mp3-128");
			uriplayer_play((char*)"http://http-live.sr.se/p4malmo-aac-192");
		}
	} else if( strcmp(argv[1],"playfile") == 0 ) {
		if( argc > 2 ) {
			TCP *pTCP;

			pTCP = connectToFirstServicing(BDP::FFRF);
			ffrf_play_file(pTCP,argv[2]);
			pTCP->close();
			delete pTCP;
		} else {

		}

	} else if( strcmp(argv[1],"stop") == 0 ) {
		uriplayer_stop();
	}

	return 0;
}

TCP *connectToFirstServicing(BDP::eServices desiredService) {
	BDP::Service *pSvc;
	int retry = 10;

	do {
		usleep(100 * 1000);
		pSvc = getFirstPeer(desiredService);
	} while( (pSvc == NULL) && (retry > 0) );

	if( pSvc == NULL ) return NULL;

	TCP *pTCP = new TCP();

	pTCP->connect(pSvc->pPeer->address,pSvc->port);

	return pTCP;
}

BDP::Service *getFirstPeer(BDP::eServices desiredService) {
	std::list<BDP::Peer *>::iterator peer;
	std::list<BDP::Peer *> peers;
	std::list<BDP::Service *>::iterator service;
	std::list<BDP::Service *> services;

	BDP *pBDP = BDP::getInstance(0);

	usleep(100 * 1000);

	peers = pBDP->getPeers();
	printf(" Found %i servers..\n",(int)peers.size());

	for(peer=peers.begin();peer!=peers.end();++peer) {
		LOG("Peer: \"%s\" has %i services",(*peer)->name,(*peer)->services.size());

		services = (*peer)->services;
		for(service=services.begin();service!=services.end();++service) {
			LOG("  Service 0x%04x @ %u",(*service)->type,(*service)->port);

			if( (*service)->type == desiredService ) return (*service);
		}
	}

	return NULL;
}
