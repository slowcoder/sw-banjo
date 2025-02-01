#pragma once

#include "media_frame.h"

struct mediainput;

struct mediainput *mediainput_avcodec_openstream(const char *pzURI);
void               mediainput_avcodec_close(struct mediainput *pzCtx);

mediaframe_t      *mediainput_avcodec_getnextframe(struct mediainput *pCtx);
