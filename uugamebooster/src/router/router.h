/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_ROUTER_H
#define UU_ROUTER_H

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#include "../config/config.h"

struct uu_router_device {
	struct device_type type;
	char interface[IF_NAMESIZE];
	char address[INET_ADDRSTRLEN];
	char network[INET_ADDRSTRLEN + 4];
	struct in_addr ip;
	unsigned char mac[ETH_ALEN];
	unsigned int index;
};

void uu_router_device_init(void);
const struct uu_router_device *uu_router_device_get(void);
const struct device_type *uu_router_device_type(void);
bool uu_router_arp_init(const struct uu_router_device *device);
bool uu_router_nat(unsigned int table, const char *tunnel);
int uu_router_route(const char *command, int (*execute)(const char *));

#endif
