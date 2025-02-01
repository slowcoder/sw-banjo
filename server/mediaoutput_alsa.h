#pragma once

#include "media_frame.h"

struct mediaoutput;

struct mediaoutput *mediaoutput_alsa_open(void);
void                mediaoutput_alsa_close(struct mediaoutput *pCtx);

int 				mediaoutput_alsa_buffer(struct mediaoutput *pCtx,mediaframe_t *pFrame);
void                mediaoutput_alsa_flush(struct mediaoutput *pCtx);

void                mediaoutput_alsa_setplaybackvolume(uint8_t volume);
