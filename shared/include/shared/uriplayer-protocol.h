#pragma once

#define URIPLAYERCMD_PLAYURI 0x01
#define URIPLAYERCMD_STOP    0x02

#pragma pack(push,1)
typedef struct {
	struct {
		uint8_t cmd;
		uint8_t payloadlen;
	} hdr;

	union {
		struct {
			char uri[0];
		} playuricmd;
	};
} uriplayerframe_t;
#pragma pack(pop)