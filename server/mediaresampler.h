#pragma once

#include "media_frame.h"

struct mediaresampler;

struct mediaresampler *mediaresampler_open(void);
void                   mediaresampler_close(struct mediaresampler *pCtx);

mediaframe_t 		  *mediaresampler_process(struct mediaresampler *pCtx,mediaframe_t *pFrame);
