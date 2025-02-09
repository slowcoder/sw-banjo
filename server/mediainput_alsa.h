#pragma once

#include "media_frame.h"

struct mediainput;

struct mediainput *mediainput_alsa_openstream(void);
void               mediainput_alsa_close(struct mediainput *pzCtx);

mediaframe_t      *mediainput_alsa_getnextframe(struct mediainput *pCtx);
