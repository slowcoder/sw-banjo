#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIAFRAME_MAX_PLANES 5

typedef enum {
	eMediasamplefmt_Invalid = 0,
	eMediasamplefmt_S16,
	eMediasamplefmt_S32,
	eMediasamplefmt_F32,
	eMediasamplefmt_MAX
} eMediasamplefmt;

typedef struct {
	int numFrames;
	int numChannels;
	uint32_t rate;
	int bIsPlanar;

	int64_t duration,pts;

	eMediasamplefmt fmt;

	void *data[MEDIAFRAME_MAX_PLANES];
} mediaframe_t;

mediaframe_t *mediaframe_alloc(void);
void          mediaframe_free(mediaframe_t *pFrame);

int           mediaframe_getsamplesize(mediaframe_t *pFrame);

#ifdef __cplusplus
}
#endif
