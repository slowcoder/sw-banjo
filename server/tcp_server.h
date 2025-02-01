#pragma once

#include "error.h"

err_code tcp_server_init(void);
err_code tcp_server_accept(int *s);
int      tcp_recv(void *pBuffer,int len,int timeoutms);
