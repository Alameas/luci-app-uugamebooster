/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_MESSAGES_H
#define UU_MESSAGES_H

#include "capture.h"

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length,
			uu_capture_writeback writeback, void *context);

#endif
