/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_MESSAGES_H
#define UU_MESSAGES_H

#include "../capture/capture.h"
#include "../proto/messages.pb-c.h"

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length,
			uu_capture_writeback writeback, void *context);

bool uu_messages_upload_enabled(enum uu_message_type type);

bool uu_messages_upload(enum uu_message_type type, const void *body, size_t length,
			void *output, size_t capacity, size_t *output_length);

#endif
