/* SPDX-License-Identifier: GPL-3.0-only */

#include <inttypes.h>
#include <json-c/json.h>
#include <libubox/ulog.h>
#include <stdio.h>

#include "messages.h"
#include "status.h"
#include "uuenv.h"

static void log_message(const char *name, const ProtobufCMessage *message);

static void log_text(const char *name, const char *value)
{
	struct json_object *env = value ? uuenv_decode(value) : NULL;

	if (env) value = json_object_to_json_string_ext(env, JSON_C_TO_STRING_PLAIN);

	ULOG_INFO("UU message %s: %s\n", name, value ? (*value ? value : "<empty>") : "<not supplied>");

	if (env) json_object_put(env);
}

static size_t field_size(ProtobufCType type)
{
	static const size_t sizes[] = {
		[PROTOBUF_C_TYPE_INT32] = sizeof(int32_t),
		[PROTOBUF_C_TYPE_SINT32] = sizeof(int32_t),
		[PROTOBUF_C_TYPE_SFIXED32] = sizeof(int32_t),
		[PROTOBUF_C_TYPE_INT64] = sizeof(int64_t),
		[PROTOBUF_C_TYPE_SINT64] = sizeof(int64_t),
		[PROTOBUF_C_TYPE_SFIXED64] = sizeof(int64_t),
		[PROTOBUF_C_TYPE_UINT32] = sizeof(uint32_t),
		[PROTOBUF_C_TYPE_FIXED32] = sizeof(uint32_t),
		[PROTOBUF_C_TYPE_UINT64] = sizeof(uint64_t),
		[PROTOBUF_C_TYPE_FIXED64] = sizeof(uint64_t),
		[PROTOBUF_C_TYPE_FLOAT] = sizeof(float),
		[PROTOBUF_C_TYPE_DOUBLE] = sizeof(double),
		[PROTOBUF_C_TYPE_BOOL] = sizeof(protobuf_c_boolean),
		[PROTOBUF_C_TYPE_ENUM] = sizeof(int),
		[PROTOBUF_C_TYPE_STRING] = sizeof(char *),
		[PROTOBUF_C_TYPE_BYTES] = sizeof(ProtobufCBinaryData),
		[PROTOBUF_C_TYPE_MESSAGE] = sizeof(ProtobufCMessage *),
	};

	return sizes[type];
}

static void log_field(const char *name, ProtobufCType type, const void *value)
{
	switch (type) {
	case PROTOBUF_C_TYPE_STRING:
		log_text(name, *(const char *const *)value);
		break;
	case PROTOBUF_C_TYPE_MESSAGE:
		if (*(const ProtobufCMessage *const *)value) {
			log_message(name, *(const ProtobufCMessage *const *)value);
		} else {
			log_text(name, NULL);
		}
		break;
	case PROTOBUF_C_TYPE_BOOL:
		ULOG_INFO("UU message %s: %s\n", name, *(const protobuf_c_boolean *)value ? "true" : "false");
		break;
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
	case PROTOBUF_C_TYPE_ENUM:
		ULOG_INFO("UU message %s: %" PRId32 "\n", name, *(const int32_t *)value);
		break;
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_FIXED32:
		ULOG_INFO("UU message %s: %" PRIu32 "\n", name, *(const uint32_t *)value);
		break;
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
		ULOG_INFO("UU message %s: %" PRId64 "\n", name, *(const int64_t *)value);
		break;
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_FIXED64:
		ULOG_INFO("UU message %s: %" PRIu64 "\n", name, *(const uint64_t *)value);
		break;
	case PROTOBUF_C_TYPE_FLOAT:
		ULOG_INFO("UU message %s: %g\n", name, (double)*(const float *)value);
		break;
	case PROTOBUF_C_TYPE_DOUBLE:
		ULOG_INFO("UU message %s: %g\n", name, *(const double *)value);
		break;
	case PROTOBUF_C_TYPE_BYTES:
		ULOG_INFO("UU message %s: bytes=%zu\n", name, ((const ProtobufCBinaryData *)value)->len);
		break;
	}
}

static void log_message(const char *name, const ProtobufCMessage *message)
{
	const char *base = (const char *)message;

	for (unsigned i = 0; i < message->descriptor->n_fields; i++) {
		const ProtobufCFieldDescriptor *field = &message->descriptor->fields[i];
		const void *value = base + field->offset;
		char path[256];

		snprintf(path, sizeof(path), "%s.%s", name, field->name);

		if (field->label == PROTOBUF_C_LABEL_REPEATED) {
			size_t count = *(const size_t *)(base + field->quantifier_offset);
			const char *items = *(const char *const *)value;

			ULOG_INFO("UU message %s: count=%zu\n", path, count);
			if (field->type == PROTOBUF_C_TYPE_BYTES) continue;

			for (size_t j = 0; j < count; j++) {
				snprintf(path, sizeof(path), "%s.%s[%zu]", name, field->name, j);
				log_field(path, field->type, items + j * field_size(field->type));
			}
		} else if (field->quantifier_offset && !*(const protobuf_c_boolean *)(base + field->quantifier_offset)) {
			log_text(path, NULL);
		} else {
			log_field(path, field->type, value);
		}
	}
}

static bool handle_check_bound_reply(const void *body, size_t length)
{
	Uu__CheckBoundReply *message = uu__check_bound_reply__unpack(NULL, length, body);

	if (!message) return false;

	log_message("CheckBoundReply", &message->base);
	if (message->has_is_bound) uu_status_bound(message->is_bound);
	uu__check_bound_reply__free_unpacked(message, NULL);
	return true;
}

static bool handle_bound_user(const void *body, size_t length)
{
	Uu__BoundUser *message = uu__bound_user__unpack(NULL, length, body);

	if (!message) return false;

	log_message("BoundUser", &message->base);
	uu__bound_user__free_unpacked(message, NULL);
	return true;
}

static bool handle_acc(const void *body, size_t length)
{
	Uu__Acc *message = uu__acc__unpack(NULL, length, body);
	bool updated;

	if (!message) return false;

	log_message("Acc", &message->base);
	updated = uu_status_update(message->deviceinfo, message);
	uu__acc__free_unpacked(message, NULL);
	return updated;
}

static bool handle_stop_acc(const void *body, size_t length)
{
	Uu__StopAcc *message = uu__stop_acc__unpack(NULL, length, body);
	bool updated;

	if (!message) return false;

	log_message("StopAcc", &message->base);
	updated = uu_status_update(message->deviceinfo, NULL);
	uu__stop_acc__free_unpacked(message, NULL);
	return updated;
}

static bool handle_wol(const void *body, size_t length)
{
	Uu__Wol *message = uu__wol__unpack(NULL, length, body);

	if (!message) return false;

	log_message("Wol", &message->base);
	uu__wol__free_unpacked(message, NULL);
	return true;
}

bool uu_messages_handle(enum uu_message_type type, const void *body, size_t length)
{
	switch (type) {
	case UU_MESSAGE_ACC:
		return handle_acc(body, length);
	case UU_MESSAGE_STOP_ACC:
		return handle_stop_acc(body, length);
	case UU_MESSAGE_BOUND_USER:
		return handle_bound_user(body, length);
	case UU_MESSAGE_CHECK_BOUND_REPLY:
		return handle_check_bound_reply(body, length);
	case UU_MESSAGE_WOL:
		return handle_wol(body, length);
	default:
		return false;
	}
}
