/* SPDX-License-Identifier: GPL-3.0-only */

#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helper.h"
#include "sni_rules.h"

static const char domain_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
static const char digit_chars[] = "0123456789";

static const char *check_regex_syntax(const char *pattern)
{
	size_t groups = 0;
	bool atom = false, repeated = false;

	for (const char *p = pattern; *p; p++) {
		if (*p == '\\') {
			p++;
			if (!*p) return "trailing escape";
			if (!strchr(".^$*+?()[]{}|\\", *p)) return "unsupported escape";
			atom = true;
			repeated = false;
		} else if (*p == '[') {
			p++;
			if (*p == '^') p++;
			if (!*p || *p == ']') return "empty or unclosed character class";

			for (; *p && *p != ']'; p++) {
				if (!strchr(domain_chars, *p)) {
					return "unsupported character class";
				}
			}
			if (!*p) return "unclosed character class";
			atom = true;
			repeated = false;
		} else if (*p == '(') {
			if (p[1] == '?') return "unsupported group extension";
			groups++;
			atom = false;
			repeated = false;
		} else if (*p == ')') {
			if (!groups) return "unmatched closing parenthesis";
			groups--;
			atom = true;
			repeated = false;
		} else if (strchr("*+?{", *p)) {
			if (!atom) return "repetition has no operand";
			if (repeated) return "nested, lazy or possessive quantifier is unsupported";

			if (*p == '{') {
				size_t digits = strspn(p + 1, digit_chars);

				if (!digits) return "invalid repetition count";
				p += digits + 1;
				if (*p == ',') p += strspn(p + 1, digit_chars) + 1;
				if (*p != '}') return "invalid or unclosed repetition count";
			}
			repeated = true;
		} else if (strchr("^$|", *p)) {
			atom = false;
			repeated = false;
		} else if (strchr(domain_chars, *p)) {
			atom = true;
			repeated = false;
		} else {
			return "unsupported regex character";
		}
	}

	return groups ? "unclosed parenthesis" : NULL;
}

static const char *parse_rule(const char *rule, char *pattern)
{
	size_t length = strnlen(rule, UU_SNI_RULE_SIZE);
	const char *end;
	char *port;

	if (!length || length == UU_SNI_RULE_SIZE) return "empty or oversized rule";

	if (strchr("+-!>", *rule)) rule++;
	if (!*rule || strchr("+-!>~ ", *rule)) return "empty expression or invalid prefix";

	end = rule + strcspn(rule, " ");
	for (const char *p = rule; p < end; p++) {
		if ((unsigned char)*p < 0x21 || (unsigned char)*p > 0x7e) {
			return "expression contains whitespace or non-ASCII characters";
		}
	}
	if (*end && !valid_ipv4_list(end + 1)) return "invalid IPv4 address list";

	memcpy(pattern, rule, (size_t)(end - rule));
	pattern[end - rule] = '\0';
	if (strchr(pattern, '_') || strchr(pattern, '#')) return "unsupported UU rule delimiter or extension";

	port = strchr(pattern, ':');
	if (port) {
		if (!valid_port(port + 1)) return "port must be an integer from 1 to 65535";
		*port = '\0';
	}
	if (!*pattern) return "empty expression";

	return NULL;
}

bool uu_sni_rule_valid(const char *rule, char *error, size_t size)
{
	char pattern[UU_SNI_RULE_SIZE];
	const char *reason = parse_rule(rule, pattern);

	if (!reason) reason = check_regex_syntax(pattern);
	if (reason) {
		snprintf(error, size, "%s", reason);
		return false;
	}

	return valid_regex(pattern, error, size);
}

static char **copy_rules(const Uu__Acc *message, size_t capacity)
{
	char **rules = malloc(capacity * sizeof(*rules));

	if (!rules) return NULL;

	for (size_t i = 0; i < message->n_sni_host; i++) {
		rules[i] = strdup(message->sni_host[i]);
		if (!rules[i]) {
			str_array_free(rules, i);
			return NULL;
		}
	}

	return rules;
}

static bool delete_rule(Uu__Acc *message, const struct uu_sni_rule *rule)
{
	regex_t regex;
	bool use_regex = rule->operation == UU_SNI_DELREG;
	bool ok = false;
	size_t count = 0;

	if (use_regex && regcomp(&regex, rule->value, REG_EXTENDED | REG_NOSUB)) return false;

	for (size_t i = 0; i < message->n_sni_host; i++) {
		char *value = message->sni_host[i];
		bool matched;

		if (use_regex) {
			int result = regexec(&regex, value, 0, NULL, 0);

			if (result != 0 && result != REG_NOMATCH) goto out;
			matched = result == 0;
		} else {
			matched = !strcmp(value, rule->value);
		}

		if (matched) {
			free(value);
			message->sni_host[i] = NULL;
		}
	}

	for (size_t i = 0; i < message->n_sni_host; i++) {
		if (message->sni_host[i]) message->sni_host[count++] = message->sni_host[i];
	}
	message->n_sni_host = count;
	ok = true;

out:
	if (use_regex) regfree(&regex);

	return ok;
}

static bool same_rules(const Uu__Acc *a, const Uu__Acc *b)
{
	if (a->n_sni_host != b->n_sni_host) return false;

	for (size_t i = 0; i < a->n_sni_host; i++) {
		if (strcmp(a->sni_host[i], b->sni_host[i])) return false;
	}

	return true;
}

int uu_sni_rules_apply(const struct uu_sni_rules *rules, Uu__Acc *message)
{
	Uu__Acc copy;

	if (!rules || !message || rules->count > UU_SNI_MAX_RULES) return -1;
	if (!rules->count) return 0;
	if (message->n_sni_host > SIZE_MAX / sizeof(char *) - rules->count) return -1;

	copy = *message;
	copy.sni_host = copy_rules(message, message->n_sni_host + rules->count);
	if (!copy.sni_host) return -1;

	for (size_t i = 0; i < rules->count; i++) {
		const struct uu_sni_rule *rule = &rules->entries[i];

		switch (rule->operation) {
		case UU_SNI_DEL:
		case UU_SNI_DELREG:
			if (!delete_rule(&copy, rule)) goto fail;
			break;
		case UU_SNI_ADD: {
			char *value;

			if (str_array_contains(copy.sni_host, copy.n_sni_host, rule->value)) break;
			value = strdup(rule->value);
			if (!value) goto fail;
			copy.sni_host[copy.n_sni_host++] = value;
			break;
		}
		default:
			goto fail;
		}
	}

	if (same_rules(message, &copy)) {
		str_array_free(copy.sni_host, copy.n_sni_host);
		return 0;
	}

	str_array_free(message->sni_host, message->n_sni_host);
	message->sni_host = copy.sni_host;
	message->n_sni_host = copy.n_sni_host;

	return 1;

fail:
	str_array_free(copy.sni_host, copy.n_sni_host);

	return -1;
}
