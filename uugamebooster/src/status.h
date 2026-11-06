/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_STATUS_H
#define UU_STATUS_H

#include <stdbool.h>
#include <stddef.h>

#include "proto/messages.pb-c.h"

bool uu_status_bound(bool bound);
bool uu_status_update(const Uu__Device *device, const Uu__Acc *message);

#endif
