/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <uci.h>

#include "config.h"
#include "helper.h"

static struct device_type *list;
static size_t count;

static struct uci_context *config_open(struct uci_package **package)
{
	struct uci_context *ctx = uci_alloc_context();

	*package = NULL;
	if (!ctx) return NULL;

	if (uci_load(ctx, "uugamebooster", package) != UCI_OK) {
		uci_free_context(ctx);
		return NULL;
	}

	return ctx;
}

void uu_config_load_devices(void)
{
	struct uci_context *ctx;
	struct uci_package *package;
	struct uci_element *element;

	free(list);
	list = NULL;
	count = 0;

	ctx = config_open(&package);
	if (!ctx) return;

	uci_foreach_element(&package->sections, element)
	{
		struct uci_section *section = uci_to_section(element);
		struct device_type *next;
		const char *mac, *type;
		size_t i;

		if (strcmp(section->type, "device_type")) continue;

		mac = uci_lookup_option_string(ctx, section, "mac");
		type = uci_lookup_option_string(ctx, section, "type");
		if (!mac || !type || !valid_mac(mac) || !valid_type(type)) continue;

		for (i = 0; i < count; i++) {
			if (!strcasecmp(list[i].mac, mac)) break;
		}

		if (i != count) continue;

		next = realloc(list, (count + 1) * sizeof(*list));
		if (!next) break;

		list = next;
		strcpy(list[count].mac, mac);
		strcpy(list[count].type, type);
		count++;
	}

	uci_free_context(ctx);
}

const struct device_type *uu_config_devices(size_t *device_count)
{
	if (device_count) *device_count = count;

	return list;
}
