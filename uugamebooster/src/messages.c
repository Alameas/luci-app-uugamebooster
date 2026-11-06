/* SPDX-License-Identifier: GPL-3.0-only */

#include <libubox/ulog.h>

#include "message_log.h"
#include "messages.h"
#include "status.h"

struct message_handler {
	const ProtobufCMessageDescriptor *descriptor;
	void (*handle)(ProtobufCMessage *message, bool *modified);
};

static const struct message_handler handlers[] = {
	[UU_MESSAGE_ACC] = { .descriptor = &uu__acc__descriptor },
	[UU_MESSAGE_STOP_ACC] = { .descriptor = &uu__stop_acc__descriptor },
	[UU_MESSAGE_BOUND_USER] = { .descriptor = &uu__bound_user__descriptor },
	[UU_MESSAGE_CHECK_BOUND_REPLY] = { .descriptor = &uu__check_bound_reply__descriptor },
	[UU_MESSAGE_WOL] = { .descriptor = &uu__wol__descriptor },
};

static bool update_status(enum uu_message_type type, ProtobufCMessage *message)
{
	switch (type) {
	case UU_MESSAGE_ACC: {
		Uu__Acc *acc = (Uu__Acc *)message;

		return uu_status_update(acc->deviceinfo, acc);
	}
	case UU_MESSAGE_STOP_ACC:
		return uu_status_update(((Uu__StopAcc *)message)->deviceinfo, NULL);
	case UU_MESSAGE_CHECK_BOUND_REPLY: {
		Uu__CheckBoundReply *reply = (Uu__CheckBoundReply *)message;

		if (reply->has_is_bound) uu_status_bound(reply->is_bound);
		break;
	}
	default:
		break;
	}

	return true;
}

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length,
			uu_capture_writeback writeback, void *context)
{
	const struct message_handler *handler;
	const ProtobufCMessageDescriptor *descriptor;
	ProtobufCMessage *message;
	bool modified = false, result;

	if ((unsigned)type >= sizeof(handlers) / sizeof(handlers[0])) return false;

	handler = &handlers[type];
	descriptor = handler->descriptor;
	if (!descriptor) return false;

	message = protobuf_c_message_unpack(descriptor, NULL, length, body);
	if (!message) return false;

	if (handler->handle) {
		handler->handle(message, &modified);
	}

	if (modified) {
		if (writeback && writeback(context, message)) {
			ULOG_INFO("UU message %s: modified and applied\n", descriptor->short_name);
		} else {
			ULOG_ERR("UU message %s: writeback failed, original retained\n", descriptor->short_name);
			protobuf_c_message_free_unpacked(message, NULL);
			message = protobuf_c_message_unpack(descriptor, NULL, length, body);
			if (!message) return false;
		}
	}

	uu_message_log(message);
	result = update_status(type, message);
	protobuf_c_message_free_unpacked(message, NULL);

	return result;
}
