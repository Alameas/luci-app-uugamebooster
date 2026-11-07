/* SPDX-License-Identifier: GPL-3.0-only */

#include "capture.h"
#include "config.h"
#include "helper.h"
#include "messages.h"

__attribute__((constructor)) static void hook_init(void)
{
	if (!is_uugamebooster()) return;

	uu_config_load_devices();
	uu_config_load_sni_rules();

	uu_capture_install(uu_messages_handle);
}
