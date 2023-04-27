/*! *********************************************************************************
 *
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef _APP_PREINCLUDE_H_
#define _APP_PREINCLUDE_H_

#ifdef __cplusplus
extern "C" {
#endif

// #define OT_ZB_SUPPORT 1      // DUAL_MODE_APP is used instead

#define TRACE_APP TRUE
#define TRACE_ZCL TRUE

#undef DBG_vPrintf
#define DBG_vPrintf(STREAM, FORMAT, ARGS...)
#ifdef __cplusplus
}
#endif

#endif /* _APP_PREINCLUDE_H_ */
