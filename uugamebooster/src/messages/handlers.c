/* SPDX-License-Identifier: GPL-3.0-only */

#include "internal.h"

const struct uu_message_handler uu_message_handlers[] = {
	[UU_MESSAGE_CONNECT] = {
		.descriptor = &uu__connect__descriptor,
		.send = uu_messages_handle_connect_send,
	},
	[UU_MESSAGE_ACC] = {
		.descriptor = &uu__acc__descriptor,
		.receive = uu_messages_handle_acc_receive,
	},
	[UU_MESSAGE_STOP_ACC] = { .descriptor = &uu__stop_acc__descriptor },
	[UU_MESSAGE_BOUND_USER] = {
		.descriptor = &uu__bound_user__descriptor,
		.receive = uu_messages_handle_bound_user_receive,
	},
	[UU_MESSAGE_CHECK_BOUND_REPLY] = { .descriptor = &uu__check_bound_reply__descriptor },
	[UU_MESSAGE_WOL] = { .descriptor = &uu__wol__descriptor },
};

const size_t uu_message_handler_count =
	sizeof(uu_message_handlers) / sizeof(uu_message_handlers[0]);
