/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_VTABLE_H
#define UU_VTABLE_H

#include <stdbool.h>

void *uu_vtable_find(const char *typeinfo, unsigned slots);
bool uu_vtable_patch(void *vtable, unsigned slot, void *function);

#endif
