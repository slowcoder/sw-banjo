#pragma once

#define FFRPCMD_READ 0x01
#define FFRPCMD_SEEK 0x02
#define FFRPCMD_DONE 0x03

#define FFRPSTATUS_PROGRESS 0x10
#define FFRPSTATUS_METADATA 0x11

#define FFRPWHENCE_SET 0x00
#define FFRPWHENCE_CUR 0x01
#define FFRPWHENCE_END 0x02
#define FFRPWHENCE_SIZE 0x10000

#define FFRPMETA_MAXSTR 64

#pragma pack(push,1)
typedef struct {
	struct {
		uint8_t cmd;
		uint8_t payloadlen;
	} hdr;

	union {
		struct {
			int len;
		} readcmd;
		struct {
			int offset;
			uint32_t whence;
		} seekcmd;
		struct {
			int64_t pts;
			int64_t duration;
		} progressstatus;
		struct {
			char title[FFRPMETA_MAXSTR];
			char album[FFRPMETA_MAXSTR];
			char artist[FFRPMETA_MAXSTR];
		} metadata;
	};
} ffrpframe_t;
#pragma pack(pop)