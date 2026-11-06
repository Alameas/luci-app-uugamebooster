/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_CODED_INPUT_H
#define UU_CODED_INPUT_H

#include <stdbool.h>
#include <stdint.h>

struct uu_coded_input {
	void *input;
	const unsigned char *buffer;
	const unsigned char *buffer_end;
	int total_bytes_read;
	int overflow_bytes;
	uint32_t last_tag;
	bool legitimate_message_end;
	bool aliasing_enabled;
	int current_limit;
	int buffer_size_after_limit;
	int total_bytes_limit;
	int total_bytes_warning_threshold;
	int recursion_depth;
	int recursion_limit;
	const void *extension_pool;
	void *extension_factory;
};

#endif
