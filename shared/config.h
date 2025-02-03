#pragma once

#if defined(__aarch64__) || defined(__arm__)
#define CFG_ALSA_PCMDEVICE    "default"
#define CFG_ALSA_MIXERDEVICE  "default"
#define CFG_ALSA_MIXERELEMENT "DIG_VOL"
#else
//#define CFG_ALSA_PCMDEVICE    "plughw:1,0"
//#define CFG_ALSA_MIXERDEVICE  "hw:1"
#define CFG_ALSA_PCMDEVICE    "plughw:0,0"
#define CFG_ALSA_MIXERDEVICE  "hw:0"
#define CFG_ALSA_MIXERELEMENT "Master"
#endif

#define CFG_OUTPUT_SAMPLERATE    48000
#define CFG_OUTPUT_BITSPERSAMPLE 32
