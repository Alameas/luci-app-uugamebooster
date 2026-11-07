/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CAPTURE_H
#define UU_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>

enum uu_message_type {
	UU_MESSAGE_ECHO_REPLY = 1,
	UU_MESSAGE_CONNECT = 2,
	UU_MESSAGE_CONNECT_REPLY = 3,
	UU_MESSAGE_ACC = 6,
	UU_MESSAGE_STOP_ACC = 8,
	UU_MESSAGE_UPGRADE = 12,
	UU_MESSAGE_MAC_LIST = 14,
	UU_MESSAGE_BOUND_USER = 16,
	UU_MESSAGE_LOG_CTRL = 18,
	UU_MESSAGE_UNINSTALL = 20,
	UU_MESSAGE_ONLINE_REPLY = 23,
	UU_MESSAGE_CONNECT_REQUEST = 30,
	UU_MESSAGE_SERVER_PING = 34,
	UU_MESSAGE_CHECK_BOUND_REPLY = 37,
	UU_MESSAGE_DISCONNECT = 38,
	UU_MESSAGE_FENCE_MODE_CTRL = 44,
	UU_MESSAGE_WOL = 50,
	UU_MESSAGE_GT_SINGLE_PLAYER_START = 52,
	UU_MESSAGE_GT_SINGLE_PLAYER_STOP = 54,
	UU_MESSAGE_REMOTE_LINK_CONFIG = 58,
	UU_MESSAGE_GT_CLICK_OFFLINE = 60,
	UU_MESSAGE_ROUTER_GATEWAY_HIJACK = 62,
};

struct ProtobufCMessage;

typedef bool (*uu_capture_writeback)(void *context, const struct ProtobufCMessage *message);
typedef bool (*uu_capture_handler)(enum uu_message_type type, const void *body, size_t length,
				   uu_capture_writeback writeback, void *context);

void uu_capture_install(uu_capture_handler handler);

#endif
