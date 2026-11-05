/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "status.h"
#include "helper.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef UU_STATUS_FILE
#define UU_STATUS_FILE "/var/run/uugamebooster.status"
#endif

static struct json_object *state, *devices;
static pid_t owner;

static void text(struct json_object *o, const char *key, const char *value)
{
	if (value) json_object_object_add(o, key, json_object_new_string(value));
}

static bool publish(void)
{
	char temporary[] = UU_STATUS_FILE ".XXXXXX";
	int fd = mkstemp(temporary);
	bool ok;

	if (fd < 0) return false;

	ok = fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 &&
	     json_object_to_fd(fd, state, JSON_C_TO_STRING_PLAIN) == 0;
	if (close(fd) < 0) ok = false;
	if (ok && rename(temporary, UU_STATUS_FILE) == 0) return true;

	unlink(temporary);
	return false;
}

bool uu_status_update(const Uu__Device *device, const Uu__Acc *message)
{
	struct json_object *d;
	const char *mac;

	mac = device ? device->mac : NULL;
	if (!mac || !valid_mac(mac)) return false;

	if (!state || owner != getpid()) {
		if (state) json_object_put(state);

		state = json_object_new_object();
		devices = json_object_new_array();
		json_object_object_add(state, "devices", devices);
		owner = getpid();
	}

	for (size_t i = 0; i < json_object_array_length(devices); i++) {
		struct json_object *value;

		json_object_object_get_ex(json_object_array_get_idx(devices, i), "mac", &value);
		if (!strcasecmp(mac, json_object_get_string(value))) {
			json_object_array_del_idx(devices, i, 1);
			break;
		}
	}

	if (message) {
		d = json_object_new_object();
		text(d, "mac", mac);

		if (device->has_ip) {
			struct in_addr address = {.s_addr = htonl(device->ip)};
			char ip[INET_ADDRSTRLEN];

			if (inet_ntop(AF_INET, &address, ip, sizeof(ip))) text(d, "ip", ip);
		}

		text(d, "hostname", device->hostname);
		text(d, "type", device->type);
		text(d, "game_id", message->game_id);
		text(d, "game", message->game);
		text(d, "zone", message->zone);
		text(d, "mode", message->mode);

		if (message->has_dual_channel) {
			json_object_object_add(d, "dual_channel",
					       json_object_new_boolean(message->dual_channel));
		}

		json_object_object_add(d, "node_count",
				       json_object_new_int64(message->n_serverlist));
		json_object_object_add(d, "route_count",
				       json_object_new_int64(message->n_routelist));
		json_object_array_add(devices, d);
	}

	return publish();
}

__attribute__((destructor)) static void status_cleanup(void)
{
	if (owner == getpid()) unlink(UU_STATUS_FILE);
}
