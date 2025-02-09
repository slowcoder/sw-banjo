#pragma once

#include <stdint.h>

typedef enum {
  eEventType_Invalid = 0,
  eEventType_Volume_Set    = 0x10,
  eEventType_Playstream    = 0x20,
  eEventType_Playback_Stop = 0x30,
  eEventType_Quit          = 0x40,
  eEventType_Switch_Input  = 0x50,
} eEventType;

typedef struct {
  eEventType type;

  union {
    struct {
      uint8_t level;
    } volume_set;
    struct {
      char pzURI[2048];
    } playstream;
    struct {
      int input; // 0=AVCodec, 1=ALSA Input
    } switch_input;
  };
} IPCEvent_t;

int IPCEvent_Post(IPCEvent_t *pEv);
int IPCEvent_Poll(IPCEvent_t *pEv);

void IPCQuitApp(void);
int  IPCWantsToQuit(void);