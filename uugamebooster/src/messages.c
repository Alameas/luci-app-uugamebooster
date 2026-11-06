/* SPDX-License-Identifier: GPL-3.0-only */

#include "message_log.h"
#include "messages.h"
#include "status.h"

static bool handle_check_bound_reply(const void *body, size_t length)
{
	Uu__CheckBoundReply *message = uu__check_bound_reply__unpack(NULL, length, body);

	if (!message) return false;

	uu_message_log(&message->base);
	if (message->has_is_bound) uu_status_bound(message->is_bound);
	uu__check_bound_reply__free_unpacked(message, NULL);
	return true;
}

static bool handle_bound_user(const void *body, size_t length)
{
	Uu__BoundUser *message = uu__bound_user__unpack(NULL, length, body);

	if (!message) return false;

	uu_message_log(&message->base);
	uu__bound_user__free_unpacked(message, NULL);
	return true;
}

static bool handle_acc(const void *body, size_t length)
{
	Uu__Acc *message = uu__acc__unpack(NULL, length, body);
	bool updated;

	if (!message) return false;

	uu_message_log(&message->base);
	updated = uu_status_update(message->deviceinfo, message);
	uu__acc__free_unpacked(message, NULL);
	return updated;
}

static bool handle_stop_acc(const void *body, size_t length)
{
	Uu__StopAcc *message = uu__stop_acc__unpack(NULL, length, body);
	bool updated;

	if (!message) return false;

	uu_message_log(&message->base);
	updated = uu_status_update(message->deviceinfo, NULL);
	uu__stop_acc__free_unpacked(message, NULL);
	return updated;
}

static bool handle_wol(const void *body, size_t length)
{
	Uu__Wol *message = uu__wol__unpack(NULL, length, body);

	if (!message) return false;

	uu_message_log(&message->base);
	uu__wol__free_unpacked(message, NULL);
	return true;
}

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length)
{
	switch (type) {
	case UU_MESSAGE_ACC:
		return handle_acc(body, length);
	case UU_MESSAGE_STOP_ACC:
		return handle_stop_acc(body, length);
	case UU_MESSAGE_BOUND_USER:
		return handle_bound_user(body, length);
	case UU_MESSAGE_CHECK_BOUND_REPLY:
		return handle_check_bound_reply(body, length);
	case UU_MESSAGE_WOL:
		return handle_wol(body, length);
	default:
		return false;
	}
}
