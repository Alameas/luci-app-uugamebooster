/* SPDX-License-Identifier: GPL-3.0-only */

#include <libubox/ulog.h>
#include <stdlib.h>
#include <string.h>

#include "message_log.h"
#include "messages.h"
#include "internal.h"

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length,
			uu_capture_writeback writeback, void *context)
{
	const struct uu_message_handler *handler;
	const ProtobufCMessageDescriptor *descriptor;
	ProtobufCMessage *message;
	bool modified = false, result;

	if ((unsigned)type >= uu_message_handler_count) return false;

	handler = &uu_message_handlers[type];
	descriptor = handler->descriptor;
	if (!descriptor) return false;

	message = protobuf_c_message_unpack(descriptor, NULL, length, body);
	if (!message) return false;

	if (handler->receive) handler->receive(message, &modified);

	if (modified) {
		if (writeback && writeback(context, message)) {
			ULOG_INFO("UU message %s: modified and applied\n", descriptor->short_name);
		} else {
			ULOG_ERR("UU message %s: writeback failed, original retained\n",
				 descriptor->short_name);
			protobuf_c_message_free_unpacked(message, NULL);
			message = protobuf_c_message_unpack(descriptor, NULL, length, body);
			if (!message) return false;
		}
	}

	uu_message_log(message);
	result = uu_messages_update_status(type, message);
	protobuf_c_message_free_unpacked(message, NULL);

	return result;
}

bool uu_messages_upload_enabled(enum uu_message_type type)
{
	if ((unsigned)type >= uu_message_handler_count) return false;

	return uu_message_handlers[type].descriptor && uu_message_handlers[type].send;
}

bool uu_messages_upload(enum uu_message_type type, const void *body, size_t length,
			void *output, size_t capacity, size_t *output_length)
{
	const struct uu_message_handler *handler;
	ProtobufCMessage *base;
	size_t packed;
	bool modified = false;

	if (!body || !output_length || (unsigned)type >= uu_message_handler_count)
		return false;

	handler = &uu_message_handlers[type];
	if (!handler->descriptor) return false;

	base = protobuf_c_message_unpack(handler->descriptor, NULL, length, body);
	if (!base) return false;

	if (output) uu_message_log(base);
	if (handler->send) handler->send(base, &modified);

	if (!modified) {
		*output_length = length;
		if (output) memcpy(output, body, length);
		protobuf_c_message_free_unpacked(base, NULL);
		return true;
	}

	packed = protobuf_c_message_get_packed_size(base);
	if (packed > capacity && output) goto error;
	*output_length = packed;
	if (output && protobuf_c_message_pack(base, output) != packed) goto error;

	protobuf_c_message_free_unpacked(base, NULL);
	return true;

error:
	protobuf_c_message_free_unpacked(base, NULL);
	return false;
}
