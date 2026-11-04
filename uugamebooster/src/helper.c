/* SPDX-License-Identifier: GPL-3.0-only */

#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "helper.h"

bool valid_mac(const char *mac)
{
	size_t i;

	if (strlen(mac) != 17) {
		return false;
	}

	for (i = 0; i < 17; i++) {
		if (i % 3 == 2) {
			if (mac[i] != ':') {
				return false;
			}
		} else if (!isxdigit((unsigned char)mac[i])) {
			return false;
		}
	}

	return true;
}

bool valid_type(const char *type)
{
	static const char *const types[] = {
		"ally", "android", "apple", "ios", "macos", "oculus", "pico", "ps4", "ps5",
		"ps_portal", "steam_deck", "switch", "switch2", "switch_cn", "tmgz",
		"vision_pro", "windows", "xbox", "xboxs", "xboxx"
	};
	size_t i;

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		if (!strcmp(type, types[i])) {
			return true;
		}
	}

	return false;
}

bool is_uugamebooster(void)
{
	char exe[128];
	ssize_t len;

	len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (len < 0 || (size_t)len >= sizeof(exe) - 1) {
		return false;
	}

	exe[len] = '\0';
	return !strcmp(exe, "/usr/bin/uugamebooster");
}
