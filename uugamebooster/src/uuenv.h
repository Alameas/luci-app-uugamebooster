/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UUENV_H
#define UUENV_H

struct json_object;

struct json_object *uuenv_decode(const char *input);
char *uuenv_merge_rules(const char *input);

#endif
