/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_HELPER_H
#define UU_HELPER_H

#include <stdbool.h>

__attribute__((visibility("hidden"))) bool valid_mac(const char *mac);
__attribute__((visibility("hidden"))) bool valid_type(const char *type);
__attribute__((visibility("hidden"))) bool is_uugamebooster(void);

#endif
