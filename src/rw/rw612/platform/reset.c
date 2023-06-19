/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_platform_ot.h"
#include <openthread/error.h>

otError otPlatResetOt(void)
{
    otError error = OT_ERROR_NONE;

    if (PLATFORM_ResetOt() < 0)
        error = OT_ERROR_FAILED;

    return error;
}
