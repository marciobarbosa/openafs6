/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include "rx.h"

#include "rx_atomic.h"
#include "rx_clock.h"
#include "rx_peer.h"

rx_in_addr_t rx_HostOf(struct rx_peer *peer) {
    rx_in_addr_t ipv4;

    if(!rx_try_sockaddr_to_ipv4(&peer->saddr, &ipv4))
    	return EAFNOSUPPORT;
    else
    	return ipv4;
}

rx_port_t rx_PortOf(struct rx_peer *peer) {
    return rx_get_sockaddr_port(&peer->saddr);
}

struct rx_sockaddr *rx_SockAddrOf(struct rx_peer *peer) {
    return &peer->saddr;
}
