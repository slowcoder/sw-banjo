#pragma once

#include <stdarg.h>
#include <assert.h>
#include <stdint.h>

#ifdef _WIN32
#define __func__ __FUNCTION__
#endif

enum {
	LOGLEVEL_ERROR,
	LOGLEVEL_WARN,
	LOGLEVEL_INFO,
	LOGLEVEL_VERB,
	LOGLEVEL_DEBUG,
};

#ifndef LOGLEVEL
#define LOGLEVEL LOGLEVEL_INFO
#endif

#ifdef __cplusplus
extern "C" {
#endif
void __logi(const char *pzFunc,const char *pzFile,int line,const char *pzMessage,...);
#ifdef __cplusplus
}
#endif

#if defined(__linux__) || defined(__PS3_GCC_REVISION__)
#define LOGx(l,x,...) \
  do { if (LOGLEVEL >= l) __logi(__func__,__FILE__,__LINE__,x,##__VA_ARGS__); } while (0)
#define LOGE(x,...) LOGx(LOGLEVEL_ERROR,x,##__VA_ARGS__)
#define LOGW(x,...) LOGx(LOGLEVEL_WARN ,x,##__VA_ARGS__)
#define LOGI(x,...) LOGx(LOGLEVEL_INFO ,x,##__VA_ARGS__)
#define LOGV(x,...) LOGx(LOGLEVEL_VERB ,x,##__VA_ARGS__)
#define LOGD(x,...) LOGx(LOGLEVEL_DEBUG,x,##__VA_ARGS__)
#define ASSERT(c,x,...) do { if( !(c) ) { LOGE(x,##__VA_ARGS__); assert(!x); } } while (0)
#else
#define LOGx(l,x,...) \
  do { if (LOGLEVEL >= l) __logi(__func__,__FILE__,__LINE__,x,__VA_ARGS__); } while (0)
#define LOGE(x,...) LOGx(LOGLEVEL_ERROR,x,__VA_ARGS__)
#define LOGW(x,...) LOGx(LOGLEVEL_WARN ,x,__VA_ARGS__)
#define LOGI(x,...) LOGx(LOGLEVEL_INFO ,x,__VA_ARGS__)
#define LOGV(x,...) LOGx(LOGLEVEL_VERB ,x,__VA_ARGS__)
#define LOGD(x,...) LOGx(LOGLEVEL_DEBUG,x,__VA_ARGS__)
#define ASSERT(c,x,...) do { if( !(c) ) { LOGE(x,__VA_ARGS__); assert(!x); } } while (0)
#endif

#define GLASSERT(x) do { int __c = glGetError(); if( __c ) { LOGE("%s GLerror:%x", x,__c); } } while (0)

#define LOG LOGI
