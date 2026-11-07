/* SPDX-License-Identifier: GPL-3.0-only */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <net/if.h>
#include <regex.h>
#include <stdlib.h>
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

bool valid_interface_name(const char *name)
{
	size_t length = name ? strnlen(name, IF_NAMESIZE) : 0;

	if (!length || length == IF_NAMESIZE) return false;
	for (size_t i = 0; i < length; i++) {
		unsigned char c = (unsigned char)name[i];

		if (c > 0x7f || (!isalnum(c) && !strchr("_.-", c))) return false;
	}

	return true;
}

bool valid_regex(const char *pattern, char *error, size_t size)
{
	regex_t regex;
	int result;

	result = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
	if (result) {
		regerror(result, &regex, error, size);
		return false;
	}

	regfree(&regex);
	if (size) error[0] = '\0';

	return true;
}

bool valid_ipv4_list(const char *addresses)
{
	char address[INET_ADDRSTRLEN];
	struct in_addr ip;

	for (;;) {
		size_t length = strcspn(addresses, ",");

		if (!length || length >= sizeof(address)) return false;

		memcpy(address, addresses, length);
		address[length] = '\0';
		if (inet_pton(AF_INET, address, &ip) != 1) return false;
		if (!addresses[length]) return true;

		addresses += length + 1;
	}
}

bool valid_port(const char *text)
{
	char *end;
	unsigned long port;

	if (*text < '0' || *text > '9') return false;

	errno = 0;
	port = strtoul(text, &end, 10);

	return !errno && !*end && port > 0 && port <= 65535;
}

bool str_array_contains(char *const *rules, size_t count, const char *rule)
{
	for (size_t i = 0; i < count; i++) {
		if (!strcmp(rules[i], rule)) return true;
	}

	return false;
}

void str_array_free(char **rules, size_t count)
{
	for (size_t i = 0; i < count; i++) free(rules[i]);
	free(rules);
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

const char *uu_helper_hostname(void)
{
	static char hostname[128];

	if (gethostname(hostname, sizeof(hostname) - 1)) return NULL;

	hostname[sizeof(hostname) - 1] = '\0';
	return hostname[0] ? hostname : NULL;
}


