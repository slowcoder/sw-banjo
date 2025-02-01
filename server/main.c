#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#include "log.h"
#include "shared/protocol.h"
#include "tcp_server.h"
#include "playback.h"

#define RECV_TIMEOUT 1000

static int recv_pkt(npkt_t **ppPkt) {
	npkt_t *pPkt;
	npkt_t pkt;
	int r;

	memset(&pkt,0,sizeof(npkt_t));

	r = tcp_recv(&pkt,sizeof(pkt.hdr),RECV_TIMEOUT);
	if( r == 0 ) {
		if( ppPkt != NULL ) *ppPkt = NULL;
		return 0;
	} 
	if( (r < 0) || (r != sizeof(pkt.hdr)) ) {
		LOGW("Client hung up");
		return -1;
	}

	pPkt = (npkt_t*)calloc(1,sizeof(pkt.hdr) + pkt.hdr.len);
	memcpy(pPkt,&pkt,sizeof(pkt.hdr)); // Copy the received header

	if( pkt.hdr.len > 0 ) {
		r = tcp_recv((uint8_t*)pPkt + sizeof(pPkt->hdr),pPkt->hdr.len,RECV_TIMEOUT);
		if( (r < 0) || (r != pPkt->hdr.len) ) {
			LOGW("Client hung up");
			return -2;
		}
	}

	if( ppPkt == NULL ) {
		free(pPkt);
	} else {
		*ppPkt = pPkt;
	}

	return r;
}

static void handle_client(int s,struct playback *pPBCtx) {
	npkt_t pkt;
	int r,is_connected = 1;

	memset(&pkt,0,sizeof(npkt_t));

	r = tcp_recv(&pkt,sizeof(pkt.hdr),1000);
	if( (r < 0) || (r != sizeof(pkt.hdr)) ) {
		fprintf(stderr,"Client hung up before handshake\n");
		return;
	}

	if( pkt.hdr.type != eNPktType_Handshake ) {
		fprintf(stderr,"Client didn't send handshake\n");
		return;
	}

	if( pkt.hdr.len != sizeof(pkt.handshake) ) {
		fprintf(stderr,"Client didn't send proper handshake\n");
		return;
	}

	r = tcp_recv(&pkt.handshake,sizeof(pkt.handshake),500);
	if( (r < 0) || (r != sizeof(pkt.handshake)) ) {
		fprintf(stderr,"Client hung up mid handshake\n");
		return;
	}

	if( pkt.handshake.protoversion != 1 ) {
		fprintf(stderr,"Client uses a different protocol-version (%u != %u)\n",pkt.handshake.protoversion,1);
		return;
	}

	printf("Client accepted.\n");

	while(is_connected) {
		npkt_t *pPkt;

		r = recv_pkt(&pPkt);
		if( r < 0 ) {
			is_connected = 0;
			break;
		}
		if( r == 0 ) continue;

		switch(pPkt->hdr.type) {
		case eNPktType_Volume_Set:
			printf("Should set volume to %u\n",pPkt->volume_set.level);
			playback_set_volume(pPBCtx,pPkt->volume_set.level);
			break;
		case eNPktType_PlayStream:
			printf("Should play stream \"%s\"\n",pPkt->playstream.pzURI);
			playback_play_stream(pPBCtx,pPkt->playstream.pzURI);
			break;
		case eNPktType_Stop:
			printf("Should stop playing\n");
			playback_stop(pPBCtx);
			break;
		default:
			printf("Unhandled pkt of type %i\n",pPkt->hdr.type);
		}
		if( pPkt != NULL ) {
			memset(pPkt,0,sizeof(pPkt->hdr));
			free(pPkt);
			pPkt = NULL;
		}
	}

}

int main(void) {
	struct playback *pPBCtx = NULL;
	err_code r;
	int done = 0;

	r = tcp_server_init();
	if( r != ERROR_OK ) {
		fprintf(stderr,"Failed to init TCP server..\n");
		return -1;
	}

	pPBCtx = playback_init();

	while(!done) {
		int cs;

		r = tcp_server_accept(&cs);
		if( r == ERROR_OK ) {
			fprintf(stderr,"Client connected\n");

			handle_client(cs,pPBCtx);
			fprintf(stderr,"Client disconnected\n");
		} else {
			fprintf(stderr,"Accept != ok ?\n");
			done = 1;
		}
	}

	playback_deinit(pPBCtx);

	return 0;
}
