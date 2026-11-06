/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "coded_input.h"
#include "message_native.h"

enum {
	VT_DELETE = 1,
	VT_NEW = 3,
	VT_CLEAR = 4,
	VT_INITIALIZED = 5,
	VT_MERGE = 7,
	VT_SIZE = 9,
	VT_SERIALIZE = 11,
};

typedef void *(*new_fn)(const void *);
typedef void (*delete_fn)(void *);
typedef void (*clear_fn)(void *);
typedef bool (*initialized_fn)(const void *);
typedef void (*merge_fn)(void *, const void *);
typedef int (*size_fn)(const void *);
typedef unsigned char *(*serialize_fn)(const void *, void *);

static bool prepare_input(struct uu_coded_input *stream, const void *completed_input,
			  const unsigned char *body, size_t length)
{
	memcpy(stream, completed_input, sizeof(*stream));
	if (stream->input || !stream->buffer || stream->buffer != stream->buffer_end ||
	    stream->total_bytes_read < 0 || stream->total_bytes_read > (int)UU_MESSAGE_MAX_BODY ||
	    stream->current_limit != stream->total_bytes_read || stream->overflow_bytes ||
	    stream->last_tag || !stream->legitimate_message_end || stream->aliasing_enabled ||
	    stream->buffer_size_after_limit || stream->recursion_depth ||
	    stream->recursion_limit <= 0 || stream->extension_pool || stream->extension_factory ||
	    stream->total_bytes_limit < stream->total_bytes_read ||
	    stream->total_bytes_limit < (int)length) {
		return false;
	}

	stream->buffer = body;
	stream->buffer_end = body + length;
	stream->total_bytes_read = (int)length;
	stream->current_limit = (int)length;
	stream->legitimate_message_end = false;

	return true;
}

static bool same_message(const ProtobufCMessageDescriptor *descriptor,
			 const unsigned char *expected, size_t length,
			 const unsigned char *actual, size_t actual_length)
{
	ProtobufCMessage *decoded = protobuf_c_message_unpack(descriptor, NULL, actual_length, actual);
	unsigned char *canonical = NULL;
	bool same = false;

	if (!decoded) return false;

	if (protobuf_c_message_get_packed_size(decoded) == length) {
		canonical = malloc(length ? length : 1);
		if (canonical && protobuf_c_message_pack(decoded, canonical) == length) {
			same = !memcmp(expected, canonical, length);
		}
	}

	free(canonical);
	protobuf_c_message_free_unpacked(decoded, NULL);

	return same;
}

static void *parse_replacement(void *self, void **vtable, uu_message_parse parse,
			       struct uu_coded_input *stream)
{
	const unsigned char *end = stream->buffer_end;
	void *replacement = ((new_fn)vtable[VT_NEW])(self);

	if (!replacement) return NULL;

	if (*(void ***)replacement != vtable || !parse(replacement, stream) ||
	    stream->buffer != end || !stream->legitimate_message_end || stream->last_tag ||
	    !((initialized_fn)vtable[VT_INITIALIZED])(replacement)) {
		((delete_fn)vtable[VT_DELETE])(replacement);
		return NULL;
	}

	return replacement;
}

static bool validate_replacement(const void *replacement, void **vtable,
				 const ProtobufCMessageDescriptor *descriptor,
				 const unsigned char *expected, size_t length)
{
	int native_length = ((size_fn)vtable[VT_SIZE])(replacement);
	unsigned char *actual;
	bool valid;

	if (native_length < 0 || (unsigned)native_length > UU_MESSAGE_MAX_BODY) return false;

	actual = malloc(native_length ? (size_t)native_length : 1);
	if (!actual) return false;

	valid = ((serialize_fn)vtable[VT_SERIALIZE])(replacement, actual) == actual + native_length &&
		same_message(descriptor, expected, length, actual, (size_t)native_length);
	free(actual);

	return valid;
}

bool uu_message_replace(void *self, const void *completed_input, uu_message_parse parse,
			const ProtobufCMessage *message)
{
	struct uu_coded_input stream;
	unsigned char *body;
	void *replacement = NULL;
	void **vtable;
	bool ok = false;
	size_t length;

	if (!self || !parse || !completed_input || !message ||
	    !protobuf_c_message_check(message)) {
		return false;
	}

	vtable = *(void ***)self;
	if (!vtable || !vtable[VT_DELETE] || !vtable[VT_NEW] || !vtable[VT_CLEAR] ||
	    !vtable[VT_INITIALIZED] || !vtable[VT_MERGE] || !vtable[VT_SIZE] || !vtable[VT_SERIALIZE]) {
		return false;
	}

	length = protobuf_c_message_get_packed_size(message);
	if (length > UU_MESSAGE_MAX_BODY) return false;

	body = malloc(length ? length : 1);
	if (!body || protobuf_c_message_pack(message, body) != length ||
	    !prepare_input(&stream, completed_input, body, length)) {
		goto out;
	}

	replacement = parse_replacement(self, vtable, parse, &stream);
	if (!replacement) goto out;

	if (!validate_replacement(replacement, vtable, message->descriptor, body, length)) {
		goto out;
	}

	((clear_fn)vtable[VT_CLEAR])(self);
	((merge_fn)vtable[VT_MERGE])(self, replacement);
	ok = true;

out:
	if (replacement) ((delete_fn)vtable[VT_DELETE])(replacement);
	free(body);

	return ok;
}
