/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_SNI_RULES_H
#define UU_SNI_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "proto/messages.pb-c.h"

#define UU_SNI_MAX_RULES 32
#define UU_SNI_RULE_SIZE 254

enum uu_sni_operation {
	UU_SNI_ADD,
	UU_SNI_DEL,
	UU_SNI_DELREG,
};

struct uu_sni_rule {
	enum uu_sni_operation operation;
	char value[UU_SNI_RULE_SIZE];
};

struct uu_sni_rules {
	size_t count;
	struct uu_sni_rule entries[UU_SNI_MAX_RULES];
};

bool uu_sni_rule_valid(const char *rule, char *error, size_t size);
int uu_sni_rules_apply(const struct uu_sni_rules *rules, Uu__Acc *message);

#endif
