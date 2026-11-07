/* SPDX-License-Identifier: GPL-3.0-only */

#include <libubox/ulog.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <uci.h>

#include "config.h"
#include "helper.h"
#include "sni_rules.h"

static struct device_type *list;
static size_t count;
static bool router_enabled;
static struct uu_sni_rules sni_rules;

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
	router_enabled = false;

	ctx = config_open(&package);
	if (!ctx) return;

	uci_foreach_element(&package->sections, element)
	{
		struct uci_section *section = uci_to_section(element);
		struct device_type *next;
		const char *mac, *type;
		size_t i;

		if (!strcmp(section->e.name, "config")) {
			const char *value = uci_lookup_option_string(ctx, section, "router_device");

			router_enabled = value && !strcmp(value, "1");
		}
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

bool uu_config_router_enabled(void)
{
	return router_enabled;
}

bool uu_config_load_sni_rules(void)
{
	struct uu_sni_rules *rules = &sni_rules;
	struct uci_context *ctx;
	struct uci_package *package;
	struct uci_section *section;
	struct uci_option *option;
	struct uci_element *entry;
	bool ok = false;

	memset(rules, 0, sizeof(*rules));

	ctx = config_open(&package);
	if (!ctx) return false;

	section = uci_lookup_section(ctx, package, "config");
	if (!section) goto out;

	option = uci_lookup_option(ctx, section, "sni_rule");
	if (!option) {
		ok = true;
		goto out;
	}
	if (option->type != UCI_TYPE_LIST) goto out;

	uci_foreach_element(&option->v.list, entry)
	{
		const char *value;
		char error[128];
		enum uu_sni_operation operation;

		if (!strncmp(entry->name, "ADD ", 4)) {
			operation = UU_SNI_ADD;
			value = entry->name + 4;
		} else if (!strncmp(entry->name, "DEL ", 4)) {
			operation = UU_SNI_DEL;
			value = entry->name + 4;
		} else if (!strncmp(entry->name, "DELREG ", 7)) {
			operation = UU_SNI_DELREG;
			value = entry->name + 7;
		} else {
			ULOG_WARN("UU SNI: skipping invalid operation: %s\n", entry->name);
			continue;
		}

		if (!*value || strnlen(value, UU_SNI_RULE_SIZE) == UU_SNI_RULE_SIZE) {
			ULOG_WARN("UU SNI: skipping empty or oversized rule: %s\n", entry->name);
			continue;
		}
		if ((operation == UU_SNI_ADD && !uu_sni_rule_valid(value, error, sizeof(error))) ||
		    (operation == UU_SNI_DELREG && !valid_regex(value, error, sizeof(error)))) {
			ULOG_WARN("UU SNI: skipping invalid rule (%s): %s\n", error, entry->name);
			continue;
		}
		if (rules->count == UU_SNI_MAX_RULES) {
			ULOG_WARN("UU SNI: rule limit reached, skipping: %s\n", entry->name);
			continue;
		}

		rules->entries[rules->count].operation = operation;
		strcpy(rules->entries[rules->count].value, value);
		rules->count++;
	}

	ok = true;

out:
	uci_free_context(ctx);
	if (!ok) memset(rules, 0, sizeof(*rules));

	return ok;
}

const struct uu_sni_rules *uu_config_sni_rules(void)
{
	return &sni_rules;
}
