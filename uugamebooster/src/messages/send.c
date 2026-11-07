/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>
#include <string.h>
#include <libubox/ulog.h>

#include "../runtime/helper.h"
#include "message_log.h"
#include "internal.h"

void uu_messages_handle_connect_send(ProtobufCMessage *base, bool *modified)
{
	Uu__Connect *message = (Uu__Connect *)base;
	const char *override;
	char *replacement;

	if (message->ssid || message->ssid_5g) return;

	override = uu_helper_hostname();
	if (!override) return;

	replacement = strdup(override);
	if (!replacement) return;

	ULOG_INFO("UU upload Connect.ssid: <not supplied> -> %s\n", override);
	free(message->ssid);
	message->ssid = replacement;
	*modified = true;
}
