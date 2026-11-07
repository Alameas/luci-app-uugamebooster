/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CONFIG_H
#define UU_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

struct device_type {
	char mac[18];
	char type[16];
};

struct uu_sni_rules;

void uu_config_load_devices(void);
const struct device_type *uu_config_devices(size_t *count);
bool uu_config_router_enabled(void);
bool uu_config_load_sni_rules(void);
const struct uu_sni_rules *uu_config_sni_rules(void);

#endif
