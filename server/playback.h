#pragma once

struct playback;

struct playback *playback_init(void);
void             playback_deinit(struct playback *pCtx);

int              playback_set_volume(struct playback *pCtx,uint8_t level);
int              playback_play_stream(struct playback *pCtx,char *pzURI);
int              playback_stop(struct playback *pCtx);
