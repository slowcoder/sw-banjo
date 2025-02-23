#include <stdio.h>
#include <string.h>

#include "shared/log.h"

#include "config.h"

int config_load(void) {
	return 0;
}

int config_get_int(const char *pzSection,const char *pzVariable,int *pRet) {

	if( strcmp(pzSection,"alsaoutput") == 0 ) {
#if defined(__aarch64__) || defined(__arm__)
		if( strcmp(pzVariable,"samplerate") == 0 ) { *pRet = 192000; return 0; }
		if( strcmp(pzVariable,"bitdepth") == 0 ) { *pRet = 32; return 0; }
#else
		if( strcmp(pzVariable,"samplerate") == 0 ) { *pRet = 48000; return 0; }
		if( strcmp(pzVariable,"bitdepth") == 0 ) { *pRet = 16; return 0; }
#endif
	}

	return -1;
}

int config_get_string(const char *pzSection,const char *pzVariable,char *pRet,int len) {

	if( strcmp(pzSection,"alsaoutput") == 0 ) {
		if( strcmp(pzVariable,"pcmdevice") == 0 ) {
#if defined(__aarch64__) || defined(__arm__)
			snprintf(pRet,len,"hw:0,0");
#else
			snprintf(pRet,len,"plughw:1,0");
#endif
			return 0;
		}
	}

	return -1;
}
