/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CAPTURE_INTERNAL_H
#define UU_CAPTURE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "capture.h"

typedef bool (*uu_capture_parse_fn)(void *self, void *input);
typedef int (*uu_capture_size_fn)(const void *self);
typedef unsigned char *(*uu_capture_serialize_fn)(const void *self, void *data);

struct uu_capture {
	const char *type_name;
	uu_capture_parse_fn wrapper;
	uu_capture_parse_fn parse;
	uu_capture_size_fn size;
	uu_capture_serialize_fn serialize;
	uu_capture_size_fn upload_size;
	uu_capture_serialize_fn upload_serialize;
};

#define UU_CAPTURE_TYPES(X) \
	X(UU_MESSAGE_ECHO_REPLY, EchoReply) \
	X(UU_MESSAGE_CONNECT, Connect) \
	X(UU_MESSAGE_CONNECT_REPLY, ConnectReply) \
	X(UU_MESSAGE_ACC, Acc) \
	X(UU_MESSAGE_STOP_ACC, StopAcc) \
	X(UU_MESSAGE_UPGRADE, Upgrade) \
	X(UU_MESSAGE_MAC_LIST, MacList) \
	X(UU_MESSAGE_BOUND_USER, BoundUser) \
	X(UU_MESSAGE_LOG_CTRL, LogCtrl) \
	X(UU_MESSAGE_UNINSTALL, Uninstall) \
	X(UU_MESSAGE_ONLINE_REPLY, OnlineReply) \
	X(UU_MESSAGE_CONNECT_REQUEST, ConnectRequest) \
	X(UU_MESSAGE_SERVER_PING, ServerPing) \
	X(UU_MESSAGE_CHECK_BOUND_REPLY, CheckBoundReply) \
	X(UU_MESSAGE_DISCONNECT, Disconnect) \
	X(UU_MESSAGE_FENCE_MODE_CTRL, FenceModeCtrl) \
	X(UU_MESSAGE_WOL, Wol) \
	X(UU_MESSAGE_GT_SINGLE_PLAYER_START, GTSinglePlayerStart) \
	X(UU_MESSAGE_GT_SINGLE_PLAYER_STOP, GTSinglePlayerStop) \
	X(UU_MESSAGE_REMOTE_LINK_CONFIG, RemoteLinkConfig) \
	X(UU_MESSAGE_GT_CLICK_OFFLINE, GTClickOffline) \
	X(UU_MESSAGE_ROUTER_GATEWAY_HIJACK, RouterGatewayHijack)

#define DECLARE_UPLOAD_WRAPPER(type, rtti) \
	int type##_upload_size(const void *self); \
	unsigned char *type##_upload_serialize(const void *self, void *data);
UU_CAPTURE_TYPES(DECLARE_UPLOAD_WRAPPER)
#undef DECLARE_UPLOAD_WRAPPER

#define DECLARE_PARSE_WRAPPER(type, rtti) \
	bool type##_parse(void *self, void *input);
UU_CAPTURE_TYPES(DECLARE_PARSE_WRAPPER)
#undef DECLARE_PARSE_WRAPPER

extern struct uu_capture uu_captures[];
extern const size_t uu_captures_count;
extern __thread bool uu_capture_inside;
extern uu_capture_handler uu_capture_message_handler;

#endif
