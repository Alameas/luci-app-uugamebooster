/* SPDX-License-Identifier: GPL-3.0-only */

#include <limits.h>
#include <stdlib.h>

#include "../messages/messages.h"
#include "internal.h"

static int upload_size(enum uu_message_type type, const void *self)
{
	struct uu_capture *capture = &uu_captures[type];
	int length = capture->size(self);
	unsigned char *body;
	size_t output_length;

	if (length < 0) return length;
	body = malloc((size_t)length ? (size_t)length : 1);
	if (!body) return length;

	if (capture->serialize(self, body) != body + length ||
	    !uu_messages_upload(type, body, (size_t)length, NULL, 0, &output_length)) {
		free(body);
		return length;
	}

	free(body);
	return output_length <= INT_MAX ? (int)output_length : length;
}

static unsigned char *upload_serialize(enum uu_message_type type, const void *self, void *data)
{
	struct uu_capture *capture = &uu_captures[type];
	int length = capture->size(self);
	unsigned char *body;
	size_t output_length;
	int capacity;

	if (length < 0) return NULL;
	body = malloc((size_t)length ? (size_t)length : 1);
	if (!body) return NULL;

	capacity = upload_size(type, self);
	if (capacity < 0 || capture->serialize(self, body) != body + length ||
	    !uu_messages_upload(type, body, (size_t)length, data,
				 (size_t)capacity, &output_length)) {
		free(body);
		return NULL;
	}

	free(body);
	return (unsigned char *)data + output_length;
}

#define DEFINE_UPLOAD(type, rtti) \
	int type##_upload_size(const void *self) \
	{ \
		return upload_size(type, self); \
	} \
	unsigned char *type##_upload_serialize(const void *self, void *data) \
	{ \
		return upload_serialize(type, self, data); \
	}
UU_CAPTURE_TYPES(DEFINE_UPLOAD)
#undef DEFINE_UPLOAD
