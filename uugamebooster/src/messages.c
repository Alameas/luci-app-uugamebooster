/* SPDX-License-Identifier: GPL-3.0-only */

#include "messages.h"
#include "status.h"

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length)
{
	bool updated;

	if (type == UU_MESSAGE_ACC) {
		Uu__Acc *message = uu__acc__unpack(NULL, length, body);

		if (!message) return false;

		updated = uu_status_update(message->deviceinfo, message);
		uu__acc__free_unpacked(message, NULL);
		return updated;
	}

	if (type == UU_MESSAGE_STOP_ACC) {
		Uu__StopAcc *message = uu__stop_acc__unpack(NULL, length, body);

		if (!message) return false;

		updated = uu_status_update(message->deviceinfo, NULL);
		uu__stop_acc__free_unpacked(message, NULL);
		return updated;
	}

	return false;
}
