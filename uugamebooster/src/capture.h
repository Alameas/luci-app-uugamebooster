/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CAPTURE_H
#define UU_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>

enum uu_message_type {
	UU_MESSAGE_ACC = 6,
	UU_MESSAGE_STOP_ACC = 8,
};

typedef bool (*uu_capture_handler)(enum uu_message_type type, const void *body, size_t length);

void uu_capture_install(uu_capture_handler handler);

#endif
