/* SPDX-License-Identifier: GPL-3.0-only */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <libubox/ulog.h>
#include <stdlib.h>

#include "capture.h"
#include "vtable.h"

#define VT_PARSE 8
#define VT_SIZE 9
#define VT_SERIALIZE 11
#define MAX_BODY (1024U * 1024U)

typedef bool (*parse_fn)(void *self, void *input);
typedef int (*size_fn)(const void *self);
typedef unsigned char *(*serialize_fn)(const void *self, void *data);

struct capture {
	const char *typeinfo;
	parse_fn wrapper;
	parse_fn parse;
	size_fn size;
	serialize_fn serialize;
};

static bool acc_parse(void *self, void *input);
static bool stop_parse(void *self, void *input);

static struct capture captures[] = {
	[UU_MESSAGE_ACC] = {
		.typeinfo = "N18uu_router_messages3AccE",
		.wrapper = acc_parse,
	},
	[UU_MESSAGE_STOP_ACC] = {
		.typeinfo = "N18uu_router_messages7StopAccE",
		.wrapper = stop_parse,
	},
};

static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;
static __thread bool inside;
static uu_capture_handler message_handler;

static void observe(enum uu_message_type type, void *self)
{
	const struct capture *capture = &captures[type];
	int length = capture->size(self);
	unsigned char *body;

	if (length <= 0 || (unsigned)length > MAX_BODY) return;

	body = malloc((size_t)length);
	if (!body) return;

	if (capture->serialize(self, body) == body + length) {
		pthread_mutex_lock(&output_lock);
		message_handler(type, body, length);
		pthread_mutex_unlock(&output_lock);
	}

	free(body);
}

static bool dispatch(enum uu_message_type type, void *self, void *input)
{
	struct capture *capture = &captures[type];
	bool parsed = capture->parse(self, input);

	if (parsed && !inside && message_handler) {
		inside = true;
		observe(type, self);
		inside = false;
	}

	return parsed;
}

static bool acc_parse(void *self, void *input)
{
	return dispatch(UU_MESSAGE_ACC, self, input);
}

static bool stop_parse(void *self, void *input)
{
	return dispatch(UU_MESSAGE_STOP_ACC, self, input);
}

static void install(struct capture *capture)
{
	void **vtable = uu_vtable_find(capture->typeinfo, VT_SERIALIZE);

	if (!vtable) {
		ULOG_ERR("UU capture: vtable not found: %s\n", capture->typeinfo);
		return;
	}

	if (vtable[VT_PARSE] == vtable[VT_SIZE] || vtable[VT_SIZE] == vtable[VT_SERIALIZE] ||
	    vtable[VT_PARSE] == vtable[VT_SERIALIZE]) {
		ULOG_ERR("UU capture: unsupported vtable: %s\n", capture->typeinfo);
		return;
	}

	capture->parse = (parse_fn)vtable[VT_PARSE];
	capture->size = (size_fn)vtable[VT_SIZE];
	capture->serialize = (serialize_fn)vtable[VT_SERIALIZE];

	if (!uu_vtable_patch(vtable, VT_PARSE, (void *)capture->wrapper)) {
		ULOG_ERR("UU capture: vtable patch failed: %s\n", capture->typeinfo);
	}
}

void uu_capture_install(uu_capture_handler handler)
{
	message_handler = handler;
	inside = true;

	for (size_t i = 0; i < sizeof(captures) / sizeof(captures[0]); i++) {
		if (captures[i].typeinfo) install(&captures[i]);
	}

	inside = false;
}
