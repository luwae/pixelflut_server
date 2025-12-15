#ifndef PFS_NET_H
#define PFS_NET_H

#include <stdint.h>

void net_start(uint16_t port);
void net_stop(void);

// Prints information about current connections.
void net_info(void);

#endif
