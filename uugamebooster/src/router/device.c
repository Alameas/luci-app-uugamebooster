/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <net/if_arp.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "router.h"
#include "../helper.h"

static struct uu_router_device router;
static struct arpreq neighbor;
static bool enabled, neighbor_added;

static void read_interface(struct ubus_request *request, int type, struct blob_attr *message)
{
	static const struct blobmsg_policy policy = { .name = "l3_device", .type = BLOBMSG_TYPE_STRING };
	struct blob_attr *value = NULL;
	const char *name;

	(void)request;
	(void)type;
	if (!message) return;
	blobmsg_parse(&policy, 1, &value, blob_data(message), blob_len(message));
	if (!value) return;

	name = blobmsg_get_string(value);
	if (!valid_interface_name(name)) return;
	strcpy(router.interface, name);
}

static bool find_interface(void)
{
	struct ubus_context *context = ubus_connect(NULL);
	uint32_t id;
	int result = -1;

	if (!context) return false;
	if (!ubus_lookup_id(context, "network.interface.lan", &id)) {
		result = ubus_invoke(context, id, "status", NULL, read_interface, NULL, 2000);
	}
	ubus_free(context);
	return !result && router.interface[0];
}

static bool read_network(int fd, struct ifreq *request)
{
	struct in_addr network;
	uint32_t mask, inverse;
	unsigned int prefix = 0;
	char address[INET_ADDRSTRLEN];

	if (ioctl(fd, SIOCGIFNETMASK, request)) return false;
	network.s_addr = ((struct sockaddr_in *)&request->ifr_netmask)->sin_addr.s_addr;
	mask = ntohl(network.s_addr);
	inverse = ~mask;
	if (!mask || (inverse & (inverse + 1))) return false;
	for (uint32_t bits = mask; bits; bits <<= 1) prefix++;
	network.s_addr &= router.ip.s_addr;
	if (!inet_ntop(AF_INET, &network, address, sizeof(address)) ||
	    !inet_ntop(AF_INET, &router.ip, router.address, sizeof(router.address))) {
		return false;
	}
	snprintf(router.network, sizeof(router.network), "%s/%u", address, prefix);
	return true;
}

static bool read_identity(int fd)
{
	struct ifreq request = { 0 };

	strcpy(request.ifr_name, router.interface);
	if (ioctl(fd, SIOCGIFADDR, &request)) return false;
	if (request.ifr_addr.sa_family != AF_INET) return false;
	router.ip = ((struct sockaddr_in *)&request.ifr_addr)->sin_addr;

	if (ioctl(fd, SIOCGIFHWADDR, &request) || request.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		return false;
	}
	memcpy(router.mac, request.ifr_hwaddr.sa_data, sizeof(router.mac));
	return read_network(fd, &request);
}

static bool add_neighbor(int fd)
{
	struct sockaddr_in *address = (struct sockaddr_in *)&neighbor.arp_pa;

	address->sin_family = AF_INET;
	address->sin_addr = router.ip;
	strcpy(neighbor.arp_dev, router.interface);
	if (!ioctl(fd, SIOCGARP, &neighbor)) {
		return (neighbor.arp_flags & ATF_COM) && !memcmp(neighbor.arp_ha.sa_data, router.mac, sizeof(router.mac));
	}
	if (errno != ENXIO && errno != ENOENT) return false;

	neighbor.arp_ha.sa_family = ARPHRD_ETHER;
	memcpy(neighbor.arp_ha.sa_data, router.mac, sizeof(router.mac));
	neighbor.arp_flags = ATF_COM | ATF_PERM;
	if (ioctl(fd, SIOCSARP, &neighbor)) return false;

	neighbor_added = true;
	return true;
}

void uu_router_device_init(void)
{
	int fd;

	if (!uu_config_router_enabled()) return;

	if (!find_interface()) return;
	router.index = if_nametoindex(router.interface);
	if (!router.index) return;

	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return;
	if (read_identity(fd) && add_neighbor(fd) && uu_router_arp_init(&router)) {
		snprintf(router.type.mac, sizeof(router.type.mac), "%02x:%02x:%02x:%02x:%02x:%02x",
			 router.mac[0], router.mac[1], router.mac[2], router.mac[3], router.mac[4], router.mac[5]);
		strcpy(router.type.type, "windows");
		enabled = true;
	}
	close(fd);
}

const struct uu_router_device *uu_router_device_get(void)
{
	return enabled ? &router : NULL;
}

const struct device_type *uu_router_device_type(void)
{
	return enabled ? &router.type : NULL;
}

__attribute__((destructor)) static void remove_neighbor(void)
{
	struct arpreq current = neighbor;
	int fd;

	if (!neighbor_added) return;
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return;
	if (!ioctl(fd, SIOCGARP, &current) && !memcmp(current.arp_ha.sa_data, router.mac, sizeof(router.mac))) {
		ioctl(fd, SIOCDARP, &current);
	}
	close(fd);
}

