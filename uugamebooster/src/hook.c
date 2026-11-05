/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <libubox/ulog.h>
#include <stdlib.h>
#include <string.h>

#include "capture.h"
#include "devices.h"
#include "helper.h"
#include "messages.h"
#include "uuenv.h"

#if defined(__aarch64__)
#define UU_FIND "_ZNKSs4findEPKcmm"
#define UU_ASSIGN "_ZNSsaSEPKc"
#else
#define UU_FIND "_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE4findEPKcjj"
#define UU_ASSIGN "_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6assignEPKc"
#endif

size_t uu_find(const void *str, const char *needle, size_t pos, size_t len) __asm__(UU_FIND);
size_t uu_find(const void *str, const char *needle, size_t pos, size_t len)
{
	static size_t (*original)(const void *, const char *, size_t, size_t);
	static void *(*assign)(void *, const char *);
	size_t result;

	if (!original) {
		original = dlsym(RTLD_NEXT, UU_FIND);
	}

	if (!original) {
		abort();
	}

	result = original(str, needle, pos, len);
	if (result == 0 && len == 6 && pos == 0 && !memcmp(needle, "UUEnv@", 6)) {
		const char *content = *(const char *const *)str;
		char *merged = content ? uuenv_merge_rules(content) : NULL;

		if (merged) {
			if (!assign) {
				assign = dlsym(RTLD_NEXT, UU_ASSIGN);
			}

			if (assign) {
				assign((void *)str, merged);
				ULOG_INFO("UUEnv device rules updated\n");
			}

			free(merged);
		}
	}

	return result;
}

__attribute__((constructor)) static void hook_init(void)
{
	if (!is_uugamebooster()) return;

	uu_devices_load();
	uu_capture_install(uu_messages_handle);
}
