#pragma once

#include <stdint.h>

int tcp_connect(char *pzServer,uint16_t port);
int tcp_recv(void *pBuffer,int len,int timeoutms);
int tcp_send(void *pBuffer,int len);
