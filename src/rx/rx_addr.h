/*
 * Copyright (c) 2014, Sine Nomine Associates
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sine Nomine Associates nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OPENAFS_RX_ADDR_H
#define OPENAFS_RX_ADDR_H

#include <rx/rx_opaque.h>

typedef int rx_bool_t;
typedef u_short rx_port_t;     /**< network byte order */
typedef u_short rx_service_t;

/**
 * IPv4 address (for compatibilty with old RPCs)
 */
typedef u_long rx_in_addr_t;
struct rx_in_addr {
    rx_in_addr_t s_addr;
};

/**
 * Generic address (IPv4 or IPv6)
 */
struct rx_address {
    int atype;		      /**< type of address: AF_INET */
    struct rx_opaque address; /**< address contents (network byte order) */
};

/**
 * Socket address for rx
 */
struct rx_sockaddr {
    rx_service_t service; /**< rx service id */
    int socktype;	  /**< socket type (SOCK_DGRAM) */
    int addrlen;    /**< addr length */
    union {
	sa_family_t family;
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_storage ss;	/* ensure alignment */
    } addr;		 /**< sockaddr */
};

/* rx_sockaddr */
#ifndef KERNEL
int rx_addrinfo_to_sockaddr(struct addrinfo *a, rx_service_t service,
			    struct rx_sockaddr *sa);
#endif
char *rx_print_sockaddr(struct rx_sockaddr *a, char *dst, size_t size);
int rx_hash_sockaddr(struct rx_sockaddr *a, int size);
int rx_compare_sockaddr(struct rx_sockaddr *a, struct rx_sockaddr *b);
int rx_is_loopback_sockaddr(struct rx_sockaddr *a);
int rx_copy_sockaddr(struct rx_sockaddr *src, struct rx_sockaddr *dst);

/* For compability with IPv4-only interfaces. */
void rx_ipv4_to_sockaddr(rx_in_addr_t ipv4, rx_port_t port,
			rx_service_t service, struct rx_sockaddr *sa);
rx_bool_t rx_try_sockaddr_to_ipv4(struct rx_sockaddr *a, rx_in_addr_t * ipv4);


/* rx_address */
char *rx_print_address(struct rx_address *a, char *dst, size_t size);
int rx_compare_address(struct rx_address *a, struct rx_address *b);
int rx_is_loopback_address(struct rx_address *a);
int rx_copy_address(struct rx_address *src, struct rx_address *dst);
int rx_free_address(struct rx_address *a);
int rx_address_to_sockaddr(struct rx_address *a, rx_port_t port,
			   rx_service_t service, struct rx_sockaddr *sa);
int rx_sockaddr_to_address(struct rx_sockaddr *sa, struct rx_address *a);

/* For compability with IPv4-only interfaces. */
int rx_ipv4_to_address(rx_in_addr_t ipv4, struct rx_address *a);
rx_bool_t rx_try_address_to_ipv4(struct rx_address *a, rx_in_addr_t * ipv4);


#endif /* OPENAFS_RX_ADDR_H */
