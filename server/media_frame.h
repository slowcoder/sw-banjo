#pragma once

#include <stdint.h>

#define MEDIAFRAME_MAX_PLANES 5

typedef enum {
	eMediasamplefmt_Invalid = 0,
	eMediasamplefmt_S16,
	eMediasamplefmt_S32,
	eMediasamplefmt_F32,
	eMediasamplefmt_MAX
} eMediasamplefmt;

typedef struct {
	int nb_samples;
	int channels;
	int32_t rate;
	int is_planar;

	eMediasamplefmt fmt;

	void *data[MEDIAFRAME_MAX_PLANES];
} mediaframe_t;


mediaframe_t *mediaframe_alloc(void);
void          mediaframe_free(mediaframe_t *pFrame);
