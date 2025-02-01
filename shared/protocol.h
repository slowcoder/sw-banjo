#pragma once

#include <stdint.h>

/* This is over TCP (control channel) */
typedef enum {
	eNPktType_Invalid = 0,
	eNPktType_Handshake,
	eNPktType_Volume_Set,
	eNPktType_PlayStream,
	eNPktType_Stop,
} eNPktType;

#pragma pack(push,1)
typedef struct {

	struct {
		uint16_t type;
		uint32_t len; // Payload length in bytes
	} hdr;

	union {
		struct {
			uint8_t protoversion;
		} handshake;
		struct {
			uint8_t level;
		} volume_set;
		struct {
			char pzURI[0];
		} playstream;
	};
} npkt_t;
#pragma pack(pop)