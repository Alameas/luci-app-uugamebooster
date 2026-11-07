/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_HELPER_H
#define UU_HELPER_H

#include <stdbool.h>
#include <stddef.h>

__attribute__((visibility("hidden"))) bool valid_mac(const char *mac);
__attribute__((visibility("hidden"))) bool valid_type(const char *type);
__attribute__((visibility("hidden"))) bool valid_interface_name(const char *name);
__attribute__((visibility("hidden"))) bool valid_ipv4_list(const char *addresses);
__attribute__((visibility("hidden"))) bool valid_port(const char *text);
__attribute__((visibility("hidden"))) bool valid_regex(const char *pattern, char *error, size_t size);
__attribute__((visibility("hidden"))) bool str_array_contains(char *const *rules, size_t count, const char *rule);
__attribute__((visibility("hidden"))) void str_array_free(char **rules, size_t count);
__attribute__((visibility("hidden"))) bool is_uugamebooster(void);
__attribute__((visibility("hidden"))) const char *uu_helper_hostname(void);

#endif
