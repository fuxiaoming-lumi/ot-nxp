/*
 *  Copyright (c) 2023, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
*  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LWIP_HOOKS_H__
#define __LWIP_HOOKS_H__

#include "lwip/ip6.h"
#include "lwip/netifapi.h"
#include <openthread/instance.h>

/**
 * @brief Initializes the lwIP hooks with the given OT instance and interfaces.
 */
void lwipHooksInit(otInstance *aInstance, struct netif *infra, struct netif *thread);

/**
 * @brief lwIP forwarding hook.
 */
int lwipCanForwardHook(ip6_addr_t *src, ip6_addr_t *dest, struct pbuf *p, struct netif *netif);

/**
 * @brief lwIP IPv6 input hook.
 *
 * @param[in] pbuf received struct pbuf passed to ip6_input()
 * @param[in] input_netif struct netif on which the packet has been received
 * @return 0 if hook has not consumed the packet, other value - hook has consumed the packet (and will free it)
 */
int lwipInputHook(struct pbuf *pbuf, struct netif *input_netif);

#endif /* __LWIP_HOOKS_H__ */
