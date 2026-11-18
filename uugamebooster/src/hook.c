/* SPDX-License-Identifier: GPL-3.0-only */

#include "capture/capture.h"
#include "config/config.h"
#include "runtime/helper.h"
#include "messages/messages.h"

__attribute__((constructor)) static void hook_init(void)
{
	if (!is_uugamebooster()) return;

	uu_config_load_devices();

	uu_capture_install(uu_messages_handle);
}
