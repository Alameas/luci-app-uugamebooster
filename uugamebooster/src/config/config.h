/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CONFIG_H
#define UU_CONFIG_H

#include <stddef.h>

struct device_type {
	char mac[18];
	char type[16];
};

void uu_config_load_devices(void);
const struct device_type *uu_config_devices(size_t *count);

#endif
