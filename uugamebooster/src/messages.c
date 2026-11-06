/* SPDX-License-Identifier: GPL-3.0-only */

#include "message_log.h"
#include "messages.h"
#include "status.h"

static const ProtobufCMessageDescriptor *message_descriptor(enum uu_message_type type)
{
	switch (type) {
	case UU_MESSAGE_ACC:
		return &uu__acc__descriptor;
	case UU_MESSAGE_STOP_ACC:
		return &uu__stop_acc__descriptor;
	case UU_MESSAGE_BOUND_USER:
		return &uu__bound_user__descriptor;
	case UU_MESSAGE_CHECK_BOUND_REPLY:
		return &uu__check_bound_reply__descriptor;
	case UU_MESSAGE_WOL:
		return &uu__wol__descriptor;
	default:
		return NULL;
	}
}

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

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length)
{
	const ProtobufCMessageDescriptor *descriptor = message_descriptor(type);
	ProtobufCMessage *message;
	bool result;

	if (!descriptor) return false;

	message = protobuf_c_message_unpack(descriptor, NULL, length, body);
	if (!message) return false;

	uu_message_log(message);
	result = update_status(type, message);
	protobuf_c_message_free_unpacked(message, NULL);

	return result;
}
