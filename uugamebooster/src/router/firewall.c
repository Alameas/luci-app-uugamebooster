/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <errno.h>
#include <libmnl/libmnl.h>
#include <nftables/libnftables.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "router.h"
#include "../runtime/helper.h"

static struct nft_ctx *nft;
static _Thread_local bool opening_nat;
static unsigned int active_table;
static pthread_mutex_t nat_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t system_once = PTHREAD_ONCE_INIT;
static int (*original_system)(const char *);

static void resolve_system(void)
{
	original_system = dlsym(RTLD_NEXT, "system");
}

struct mnl_socket *mnl_socket_open(int bus)
{
	return mnl_socket_open2(bus, opening_nat ? SOCK_CLOEXEC : 0);
}

__attribute__((destructor)) static void close_nat(void)
{
	if (nft) nft_ctx_free(nft);
	nft = NULL;
	active_table = 0;
}

static bool open_nat(void)
{
	if (nft) return true;

	opening_nat = true;
	nft = nft_ctx_new(NFT_CTX_DEFAULT);
	opening_nat = false;
	if (!nft) return false;
	nft_ctx_buffer_output(nft);
	nft_ctx_buffer_error(nft);
	if (!nft_run_cmd_from_buffer(nft,
		"create table ip uu_router_share { flags owner; }; "
		"add chain ip uu_router_share postrouting { type nat hook postrouting priority 48; policy accept; }")) {
		return true;
	}
	close_nat();
	return false;
}

static bool update_nat(unsigned int table, const char *tunnel)
{
	const struct uu_router_device *router = uu_router_device_get();
	char *commands;
	bool ok;

	if (!tunnel) {
		if (!nft || table != active_table) return true;
		if (nft_run_cmd_from_buffer(nft, "delete table ip uu_router_share")) return false;
		close_nat();
		return true;
	}

	if (!valid_interface_name(tunnel)) return false;
	if (asprintf(&commands,
		     "flush chain ip uu_router_share postrouting; "
		     "add rule ip uu_router_share postrouting ip saddr %s oifname \"%s\" counter snat to %s; ",
		     router->network, tunnel, router->address) < 0) {
		return false;
	}
	ok = open_nat() && !nft_run_cmd_from_buffer(nft, commands);
	free(commands);
	if (ok) active_table = table;
	return ok;
}

bool uu_router_nat(unsigned int table, const char *tunnel)
{
	bool result;

	pthread_mutex_lock(&nat_lock);
	result = update_nat(table, tunnel);
	pthread_mutex_unlock(&nat_lock);
	return result;
}

static char *rewrite_source(const char *command)
{
	const struct uu_router_device *router = uu_router_device_get();
	const char *source, *interface, *end;
	char *replacement;
	size_t length;

	if (strncmp(command, "iptables ", 9) && strncmp(command, "XTABLES_LIBDIR=/lib iptables ", 27)) {
		return NULL;
	}
	if (!strstr(command, " -I PREROUTING ") && !strstr(command, " -D PREROUTING ")) {
		return NULL;
	}
	interface = strstr(command, " -i ");
	source = strstr(command, " -s ");
	if (!interface || !source) return NULL;
	interface += 4;
	length = strcspn(interface, " \t\r\n");
	if (length != strlen(router->interface) || memcmp(interface, router->interface, length)) return NULL;

	source += 4;
	end = source + strcspn(source, " \t\r\n");
	length = strlen(router->address);
	if ((size_t)(end - source) != length && (size_t)(end - source) != length + 3) return NULL;
	if (memcmp(source, router->address, length) ||
	    ((size_t)(end - source) > length && memcmp(source + length, "/32", 3))) {
		return NULL;
	}
	if (asprintf(&replacement, "%.*s%s%s", (int)(source - command), command, router->network, end) < 0) {
		return NULL;
	}
	return replacement;
}

int system(const char *command)
{
	char *replacement;
	int result;

	pthread_once(&system_once, resolve_system);
	if (!original_system) {
		errno = ENOSYS;
		return -1;
	}
	if (!command || !uu_router_device_get()) return original_system(command);

	result = uu_router_route(command, original_system);
	if (result == -2) {
		replacement = rewrite_source(command);
		result = original_system(replacement ? replacement : command);
		free(replacement);
	}
	return result;
}
