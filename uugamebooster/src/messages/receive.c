/* SPDX-License-Identifier: GPL-3.0-only */

#include <libubox/ulog.h>
#include <stdlib.h>

#include "../config/config.h"
#include "../config/sni_rules.h"
#include "../config/uuenv.h"
#include "../runtime/status.h"
#include "internal.h"

void uu_messages_handle_acc_receive(ProtobufCMessage *base, bool *modified)
{
	Uu__Acc *message = (Uu__Acc *)base;

	if (!uu_config_load_sni_rules()) {
		ULOG_ERR("UU SNI: could not load rules before Acc\n");
	}

	int changed = uu_sni_rules_apply(uu_config_sni_rules(), message);

	if (changed < 0) {
		ULOG_ERR("UU Acc: could not apply SNI rules, original retained\n");
	} else if (changed > 0) {
		*modified = true;
	}
}

void uu_messages_handle_bound_user_receive(ProtobufCMessage *base, bool *modified)
{
	Uu__BoundUser *message = (Uu__BoundUser *)base;

	for (size_t i = 0; i < message->n_hostname_blacklist; i++) {
		char *merged = uuenv_merge_rules(message->hostname_blacklist[i]);

		if (!merged) continue;

		free(message->hostname_blacklist[i]);
		message->hostname_blacklist[i] = merged;
		*modified = true;
	}
}

bool uu_messages_update_status(enum uu_message_type type, ProtobufCMessage *message)
{
	switch (type) {
	case UU_MESSAGE_ACC: {
		Uu__Acc *acc = (Uu__Acc *)message;

		return uu_status_update(acc->deviceinfo, acc);
	}
	case UU_MESSAGE_STOP_ACC:
		return uu_status_update(((Uu__StopAcc *)message)->deviceinfo, NULL);
	case UU_MESSAGE_CHECK_BOUND_REPLY: {
		Uu__CheckBoundReply *reply = (Uu__CheckBoundReply *)message;

		if (reply->has_is_bound) uu_status_bound(reply->is_bound);
		break;
	}
	default:
		break;
	}

	return true;
}
