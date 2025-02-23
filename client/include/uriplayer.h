#pragma once

#include "shared/tcp.h"

int uriplayer_play_uri(TCP *pTCP,const char *pzURI);
int uriplayer_stop(TCP *pTCP);
