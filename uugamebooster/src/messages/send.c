/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libubox/ulog.h>

#include "../config/config.h"
#include "../runtime/helper.h"
#include "message_log.h"
#include "internal.h"

static Uu__Device *find_device(Uu__DeviceList *message, const char *mac)
{
	for (size_t i = 0; i < message->n_devices; i++) {
		if (message->devices[i] && message->devices[i]->mac &&
		    !strcasecmp(message->devices[i]->mac, mac))
			return message->devices[i];
	}

	return NULL;
}

static bool override_device_type(Uu__Device *device, const struct device_type *type)
{
	if (!device || !type || (device->type && !strcmp(device->type, type->type))) return false;

	ULOG_INFO("UU upload DeviceList.devices.type: %s (%s) -> %s\n",
		  type->mac, device->type ? device->type : "<empty>", type->type);
	return uu_helper_replace_string(&device->type, type->type);
}

static bool override_device_types(Uu__DeviceList *message)
{
	const struct device_type *devices;
	size_t device_count;
	bool modified = false;

	devices = uu_config_devices(&device_count);
	for (size_t i = 0; i < device_count; i++) {
		modified |= override_device_type(find_device(message, devices[i].mac), &devices[i]);
	}

	return modified;
}

void uu_messages_handle_device_list_send(ProtobufCMessage *base, bool *modified)
{
	Uu__DeviceList *message = (Uu__DeviceList *)base;

	*modified |= override_device_types(message);
}

void uu_messages_handle_connect_send(ProtobufCMessage *base, bool *modified)
{
	Uu__Connect *message = (Uu__Connect *)base;
	const char *override;

	if ((message->ssid && message->ssid[0]) ||
	    (message->ssid_5g && message->ssid_5g[0])) {
		return;
	}

	override = uu_helper_hostname();
	if (!override) return;

	ULOG_INFO("UU upload Connect.ssid: <empty> -> %s\n", override);
	*modified |= uu_helper_replace_string(&message->ssid, override);
}
