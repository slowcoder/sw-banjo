#pragma once

#include <stdint.h>

#define BDPP_TYPE_ADVERTISE  0
#define BDPP_TYPE_SERVICEREQ 1

#define BDPP_VERSION 0

#define BDPP_SVC_URIPLAYER (1<<8)
#define BDPP_SVC_SPSINK    (2<<8)
#define BDPP_SVC_FFRF      (3<<8)

#pragma pack(push,1)
typedef struct {
  uint16_t svc;
  uint16_t port;
} bdpservice_t;
typedef struct {
  uint8_t  type;
  uint8_t  version;
  uint64_t uuid;
  char     name[64];

  bdpservice_t svc[];

} bdppkt_t;
#pragma pack(pop)
