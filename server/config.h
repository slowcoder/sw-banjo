#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int config_load(void);
int config_get_int(const char *pzSection,const char *pzVariable,int *pRet);
int config_get_string(const char *pzSection,const char *pzVariable,char *pRet,int len);

#ifdef __cplusplus
}
#endif
