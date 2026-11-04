/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_DEVICES_H
#define UU_DEVICES_H

#include <stddef.h>

struct device_type {
	char mac[18];
	char type[16];
};

void uu_devices_load(void);
const struct device_type *uu_devices(size_t *count);

#endif
