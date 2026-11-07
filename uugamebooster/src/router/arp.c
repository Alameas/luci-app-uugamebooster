/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <libubox/ulog.h>
#include <libubox/utils.h>
#include <linux/filter.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "router.h"

_Static_assert(sizeof(struct ether_arp) == 28, "ARP message size");

static const struct uu_router_device *router;
static pthread_once_t symbols_once = PTHREAD_ONCE_INIT;
static int (*original_bind)(int, const struct sockaddr *, socklen_t);
static ssize_t (*original_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);

static void resolve_symbols(void)
{
	original_bind = dlsym(RTLD_NEXT, "bind");
	original_sendto = dlsym(RTLD_NEXT, "sendto");
}

bool uu_router_arp_init(const struct uu_router_device *device)
{
	pthread_once(&symbols_once, resolve_symbols);
	if (!original_bind || !original_sendto) return false;

	router = device;
	return true;
}

static bool enable_replies(int fd)
{
	struct sock_filter code[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SKF_AD_OFF + SKF_AD_PROTOCOL),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ETHERTYPE_ARP, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET | BPF_K, 0),
	};
	struct sock_fprog filter = { ARRAY_SIZE(code), code };

	return !setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
}

static bool read_lan_target(int fd, const struct sockaddr *address, socklen_t length,
		       struct sockaddr_ll *target)
{
	int type;
	socklen_t size = sizeof(type);

	if (!router || !address || length < sizeof(*target) || address->sa_family != AF_PACKET) {
		return false;
	}
	memcpy(target, address, sizeof(*target));
	return (unsigned int)target->sll_ifindex == router->index &&
		!getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) && type == SOCK_DGRAM;
}

static bool read_request(const void *data, size_t length, struct ether_arp *request)
{
	if (!data || length < sizeof(*request)) return false;

	memcpy(request, data, sizeof(*request));
	return request->arp_hrd == htons(ARPHRD_ETHER) && request->arp_pro == htons(ETHERTYPE_IP) &&
		request->arp_hln == ETH_ALEN && request->arp_pln == sizeof(struct in_addr) &&
		request->arp_op == htons(ARPOP_REQUEST);
}

static void send_reply(const struct ether_arp *request, struct sockaddr_ll target)
{
	struct ether_arp reply = { .ea_hdr = request->ea_hdr };
	int fd = socket(AF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	if (fd < 0) {
		ULOG_ERR("UU router device: socket failed: %s\n", strerror(errno));
		return;
	}

	reply.arp_op = htons(ARPOP_REPLY);
	memcpy(reply.arp_sha, router->mac, ETH_ALEN);
	memcpy(reply.arp_spa, &router->ip, sizeof(router->ip));
	memcpy(reply.arp_tha, request->arp_sha, ETH_ALEN);
	memcpy(reply.arp_tpa, request->arp_spa, sizeof(reply.arp_tpa));

	target.sll_halen = ETH_ALEN;
	memcpy(target.sll_addr, request->arp_sha, ETH_ALEN);
	original_sendto(fd, &reply, sizeof(reply), 0, (struct sockaddr *)&target, sizeof(target));
	close(fd);
}

int bind(int fd, const struct sockaddr *address, socklen_t length)
{
	struct sockaddr_ll target;
	int unused = 0;

	pthread_once(&symbols_once, resolve_symbols);
	if (!original_bind) {
		errno = ENOSYS;
		return -1;
	}
	if (!read_lan_target(fd, address, length, &target) || target.sll_protocol != htons(ETHERTYPE_ARP) ||
	    !enable_replies(fd)) {
		return original_bind(fd, address, length);
	}

	target.sll_protocol = htons(ETH_P_ALL);
	if (!original_bind(fd, (struct sockaddr *)&target, sizeof(target))) return 0;

	setsockopt(fd, SOL_SOCKET, SO_DETACH_FILTER, &unused, sizeof(unused));
	return original_bind(fd, address, length);
}

ssize_t sendto(int fd, const void *data, size_t length, int flags,
	       const struct sockaddr *address, socklen_t address_length)
{
	struct ether_arp request;
	struct sockaddr_ll target;
	ssize_t result;
	int saved_errno;

	pthread_once(&symbols_once, resolve_symbols);
	if (!original_sendto) {
		errno = ENOSYS;
		return -1;
	}
	if (!read_lan_target(fd, address, address_length, &target) ||
	    !read_request(data, length, &request)) {
		return original_sendto(fd, data, length, flags, address, address_length);
	}

	target.sll_protocol = htons(ETHERTYPE_ARP);
	result = original_sendto(fd, data, length, flags, (struct sockaddr *)&target, sizeof(target));
	if (result >= 0 && !memcmp(request.arp_tpa, &router->ip, sizeof(router->ip))) {
		saved_errno = errno;
		send_reply(&request, target);
		errno = saved_errno;
	}

	return result;
}
