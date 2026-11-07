/* SPDX-License-Identifier: GPL-3.0-only */

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libubox/ulog.h>

#include "../messages/message_native.h"
#include "internal.h"

#define MAX_BODY UU_MESSAGE_MAX_BODY

struct writeback_context {
	void *self;
	const void *input;
	uu_capture_parse_fn parse;
};

static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_type(enum uu_message_type type)
{
	if (type == UU_MESSAGE_ECHO_REPLY) return;

	ULOG_INFO("UU message received: %u\n", (unsigned)type);
}

static bool writeback_message(void *context, const struct ProtobufCMessage *message)
{
	struct writeback_context *call = context;

	return uu_message_replace(call->self, call->input, call->parse, message);
}

static void observe(enum uu_message_type type, void *self, const void *input)
{
	const struct uu_capture *capture = &uu_captures[type];
	struct writeback_context call = {
		.self = self,
		.input = input,
		.parse = capture->parse,
	};
	int length = capture->size(self);
	unsigned char *body;

	if (length < 0 || (unsigned)length > MAX_BODY) return;

	body = malloc(length ? (size_t)length : 1);
	if (!body) return;

	if (capture->serialize(self, body) == body + length) {
		pthread_mutex_lock(&output_lock);
		if (uu_capture_message_handler)
			uu_capture_message_handler(type, body, (size_t)length,
						   writeback_message, &call);
		pthread_mutex_unlock(&output_lock);
	}

	free(body);
}

static bool dispatch(enum uu_message_type type, void *self, void *input)
{
	struct uu_capture *capture = &uu_captures[type];
	bool parsed = capture->parse(self, input);

	if (parsed && !uu_capture_inside) {
		uu_capture_inside = true;
		log_type(type);
		if (uu_capture_message_handler) observe(type, self, input);
		uu_capture_inside = false;
	}

	return parsed;
}

#define DEFINE_PARSE(type, rtti) \
	bool type##_parse(void *self, void *input) \
	{ \
		return dispatch(type, self, input); \
	}
UU_CAPTURE_TYPES(DEFINE_PARSE)
#undef DEFINE_PARSE
