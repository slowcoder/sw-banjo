#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "shared/protocol.h"
#include "tcp_client.h"

static void handshake(void) {
	npkt_t pkt;

	pkt.hdr.type = eNPktType_Handshake;
	pkt.hdr.len  = sizeof(pkt.handshake);
	pkt.handshake.protoversion = 1;

	tcp_send(&pkt,sizeof(pkt.handshake) + sizeof(pkt.hdr));
}

static void volume_set(uint8_t level) {
	npkt_t pkt;

	pkt.hdr.type = eNPktType_Volume_Set;
	pkt.hdr.len  = sizeof(pkt.volume_set);
	pkt.volume_set.level = level;

	printf("Setting volume to %u\n",level);

	tcp_send(&pkt,sizeof(pkt.volume_set) + sizeof(pkt.hdr));
}

static void stop(void) {
	npkt_t pkt;

	pkt.hdr.type = eNPktType_Stop;
	pkt.hdr.len  = 0;

	tcp_send(&pkt,sizeof(pkt.hdr));
}

static void quit(void) {
	npkt_t pkt;

	pkt.hdr.type = eNPktType_Quit;
	pkt.hdr.len  = 0;

	tcp_send(&pkt,sizeof(pkt.hdr));
}

static void play_stream(char *pzURI) {
	npkt_t *pPkt;

	pPkt = (npkt_t*)calloc(1,sizeof(npkt_t) + strlen(pzURI) + 1);

	pPkt->hdr.type = eNPktType_PlayStream;
	pPkt->hdr.len  = strlen(pzURI) + 1;
	strcpy(pPkt->playstream.pzURI,pzURI);

	tcp_send(pPkt,pPkt->hdr.len + sizeof(pPkt->hdr));	
}

#define FILE_BUFFER_SZ 2048

static void play_file(char *pzFilename) {
	FILE *in;
	long fsize,c;
	npkt_t *pPkt;

	in = fopen(pzFilename,"rb");
	if( in == NULL ) return;
	fseek(in,0,SEEK_END);
	fsize = ftell(in);
	fseek(in,0,SEEK_SET);

	pPkt = (npkt_t*)calloc(1,sizeof(npkt_t));

	pPkt->hdr.type = eNPktType_PlayFile;
	pPkt->hdr.len  = sizeof(pPkt->playfile);
	pPkt->playfile.filelen = fsize;

	// Send header
	tcp_send(pPkt,pPkt->hdr.len + sizeof(pPkt->hdr));

	// Send file contents
	uint8_t buff[FILE_BUFFER_SZ];
	c = 0;
	while(c<fsize) {
		if( (c+FILE_BUFFER_SZ) < fsize ) {
			fread(buff,1,FILE_BUFFER_SZ,in);
			tcp_send(buff,FILE_BUFFER_SZ);
			c += FILE_BUFFER_SZ;
		} else {
			fread(buff,1,fsize-c,in);
			tcp_send(buff,fsize-c);
			c = fsize;
		}
	}
	fclose(in);
}

int main(int argc,char **argv) {

	if( argc < 2 ) {
		fprintf(stderr,"Usage: %s cmd [arg]\n",argv[0]);
		fprintf(stderr," Available commands:\n");
		fprintf(stderr,"   playuri [uri to stream]\n");
		fprintf(stderr,"   stop\n");
		return 0;
	}

	tcp_connect("localhost",4800);
//	tcp_connect("10.0.0.151",4800);

	handshake();

	if( strcmp(argv[1],"playuri") == 0 ) {
		if( argc > 2 ) {
			play_stream(argv[2]);
		} else {
			printf("Playing default stream\n");
			//play_stream("http://http-live.sr.se/p3-mp3-128");
			play_stream("http://http-live.sr.se/p4malmo-aac-192");
		}
	} else if( strcmp(argv[1],"playfile") == 0 ) {
		if( argc > 2 ) {
			play_file(argv[2]);
		} else {
			printf("Missing filename\n");
		}
	} else if( strcmp(argv[1],"stop") == 0 ) {
		stop();
	} else if( strcmp(argv[1],"quit") == 0 ) {
		quit();
	} else if( strcmp(argv[1],"volume") == 0 ) {
		volume_set(atoi(argv[2]));
	}

	return 0;
}
