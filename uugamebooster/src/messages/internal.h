/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_MESSAGES_INTERNAL_H
#define UU_MESSAGES_INTERNAL_H

#include <stddef.h>

#include "messages.h"
#include "../proto/messages.pb-c.h"

struct uu_message_handler {
	const ProtobufCMessageDescriptor *descriptor;
	void (*receive)(ProtobufCMessage *message, bool *modified);
	void (*send)(ProtobufCMessage *message, bool *modified);
};

extern const struct uu_message_handler uu_message_handlers[];
extern const size_t uu_message_handler_count;

void uu_messages_handle_acc_receive(ProtobufCMessage *message, bool *modified);
void uu_messages_handle_bound_user_receive(ProtobufCMessage *message, bool *modified);
bool uu_messages_update_status(enum uu_message_type type, ProtobufCMessage *message);

#endif
