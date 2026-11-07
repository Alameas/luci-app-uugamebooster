/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_MESSAGE_NATIVE_H
#define UU_MESSAGE_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#include <protobuf-c/protobuf-c.h>

#define UU_MESSAGE_MAX_BODY (1024U * 1024U)

typedef bool (*uu_message_parse)(void *self, void *input);

bool uu_message_replace(void *self, const void *completed_input, uu_message_parse parse,
			const ProtobufCMessage *message);

#endif
