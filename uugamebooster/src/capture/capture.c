/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdio.h>
#include <string.h>
#include <libubox/ulog.h>

#include "../messages/messages.h"
#include "../runtime/vtable.h"
#include "internal.h"

#define VT_PARSE 8
#define VT_SIZE 9
#define VT_SERIALIZE 11

#define CAPTURE_ENTRY(type, rtti) \
	[type] = { \
		.type_name = #rtti, \
		.wrapper = type##_parse, \
		.upload_size = type##_upload_size, \
		.upload_serialize = type##_upload_serialize, \
	},
struct uu_capture uu_captures[] = {
	UU_CAPTURE_TYPES(CAPTURE_ENTRY)
};
const size_t uu_captures_count = sizeof(uu_captures) / sizeof(uu_captures[0]);
#undef CAPTURE_ENTRY

__thread bool uu_capture_inside;
uu_capture_handler uu_capture_message_handler;

static void install(struct uu_capture *capture)
{
	char typeinfo[128];
	void **vtable;
	enum uu_message_type type = (enum uu_message_type)(capture - uu_captures);

	snprintf(typeinfo, sizeof(typeinfo), "N18uu_router_messages%zu%sE",
		 strlen(capture->type_name), capture->type_name);
	vtable = uu_vtable_find(typeinfo, VT_SERIALIZE);

	if (!vtable) {
		ULOG_ERR("UU capture: vtable not found: %s\n", typeinfo);
		return;
	}

	if (vtable[VT_PARSE] == vtable[VT_SIZE] || vtable[VT_SIZE] == vtable[VT_SERIALIZE] ||
	    vtable[VT_PARSE] == vtable[VT_SERIALIZE]) {
		ULOG_ERR("UU capture: unsupported vtable: %s\n", typeinfo);
		return;
	}

	capture->parse = (uu_capture_parse_fn)vtable[VT_PARSE];
	capture->size = (uu_capture_size_fn)vtable[VT_SIZE];
	capture->serialize = (uu_capture_serialize_fn)vtable[VT_SERIALIZE];

	if (!uu_vtable_patch(vtable, VT_PARSE, (void *)capture->wrapper)) {
		ULOG_ERR("UU capture: vtable patch failed: %s\n", typeinfo);
		return;
	}

	if (uu_messages_upload_enabled(type) &&
	    (!uu_vtable_patch(vtable, VT_SIZE, (void *)capture->upload_size) ||
	     !uu_vtable_patch(vtable, VT_SERIALIZE, (void *)capture->upload_serialize))) {
		ULOG_ERR("UU capture: upload vtable patch failed: %s\n", typeinfo);
	}
}

void uu_capture_install(uu_capture_handler handler)
{
	uu_capture_message_handler = handler;
	uu_capture_inside = true;

	for (size_t i = 0; i < uu_captures_count; i++) {
		if (uu_captures[i].type_name) install(&uu_captures[i]);
	}

	uu_capture_inside = false;
}
