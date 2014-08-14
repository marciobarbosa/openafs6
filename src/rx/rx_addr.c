/*
 * Copyright (c) 2014, Sine Nomine Associates
 * All rights reserved.
 *
 * Portions Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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

#include <afsconfig.h>
#include <afs/param.h>

#ifdef KERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"

#else
#include <roken.h>
#endif

#include <rx/rx.h>
#include <rx/rx_addr.h>


#ifndef KERNEL

static_inline void
rxi_addr_error(int error)
{
    errno = error;
}

static char *
rxi_inet_ntop_v4(const void *src, char *dst, size_t size)
{
    return (char *)inet_ntop(AF_INET, src, dst, size);
}

#else /* KERNEL */

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN  16
#endif

static_inline void
rxi_addr_error(int error)
{
}

/* inet_ntop for ipv4 */
/* Adapted from Heimdal libroken. */
static char *
rxi_inet_ntop_v4(void *src, char *dst, size_t size)
{
    const char digits[] = "0123456789";
    int i;
    struct in_addr *addr = (struct in_addr *)src;
    u_long a = ntohl(addr->s_addr);
    char *orig_dst = dst;

    if (size < INET_ADDRSTRLEN) {
	rxi_addr_error(ENOSPC);
	return NULL;
    }

    for (i = 0; i < 4; ++i) {
	int n = (a >> (24 - i * 8)) & 0xFF;
	int non_zerop = 0;

	if (non_zerop || n / 100 > 0) {
	    *dst++ = digits[n / 100];
	    n %= 100;
	    non_zerop = 1;
	}
	if (non_zerop || n / 10 > 0) {
	    *dst++ = digits[n / 10];
	    n %= 10;
	    non_zerop = 1;
	}
	*dst++ = digits[n];
	if (i != 3)
	    *dst++ = '.';
    }
    *dst++ = '\0';
    return orig_dst;
}

#ifdef HAVE_IPV6

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/* inet_ntop for ipv4 */
/* Adapted from Heimdal libroken. */
#if 0 /* to be used when ipv6 is supported in this file */
static char *
rxi_inet_ntop_v6(const void *src, char *dst, size_t size)
{
    const char xdigits[] = "0123456789abcdef";
    int i;
    const struct in6_addr *addr = (struct in6_addr *)src;
    const u_char *ptr = addr->s6_addr;
    char *orig_dst = dst;
    int compressed = 0;

    if (size < INET6_ADDRSTRLEN) {
	rxi_addr_error(ENOSPC);
	return NULL;
    }
    for (i = 0; i < 8; ++i) {
	int non_zerop = 0;

	if (compressed == 0 &&
	    ptr[0] == 0 && ptr[1] == 0 &&
	    i <= 5 &&
	    ptr[2] == 0 && ptr[3] == 0 && ptr[4] == 0 && ptr[5] == 0) {

	    compressed = 1;

	    if (i == 0)
		*dst++ = ':';
	    *dst++ = ':';

	    for (ptr += 6, i += 3;
		 i < 8 && ptr[0] == 0 && ptr[1] == 0; ++i, ptr += 2);

	    if (i >= 8)
		break;
	}

	if (non_zerop || (ptr[0] >> 4)) {
	    *dst++ = xdigits[ptr[0] >> 4];
	    non_zerop = 1;
	}
	if (non_zerop || (ptr[0] & 0x0F)) {
	    *dst++ = xdigits[ptr[0] & 0x0F];
	    non_zerop = 1;
	}
	if (non_zerop || (ptr[1] >> 4)) {
	    *dst++ = xdigits[ptr[1] >> 4];
	    non_zerop = 1;
	}
	*dst++ = xdigits[ptr[1] & 0x0F];
	if (i != 7)
	    *dst++ = ':';
	ptr += 2;
    }
    *dst++ = '\0';
    return orig_dst;
}
#endif /* if 0 - until ipv6 is supported */
#endif /* HAVE_IPV6 */
#endif /* KERNEL */

static_inline int
rxi_is_loopback_ipv4(rx_in_addr_t addr)
{
    return ((addr & 0xffff0000) == 0x7f000000);
}

/**
 * Print the address and port to a buffer as a readable string.
 *
 * \param[in]  a     address to be printed
 * \param[out] dst   buffer to be filled
 * \param[in]  size  number of bytes available in dst
 */
char *
rx_print_sockaddr(struct rx_sockaddr *sa, char *dst, size_t size)
{
    char *p;

    switch (sa->addrtype) {
    case AF_INET:
	if (size < INET_ADDRSTRLEN + 6) {
	    rxi_addr_error(ENOSPC);
	    return NULL;
	}
	rxi_inet_ntop_v4(&sa->rxsa_in_addr, dst, size);
	for (p = dst; *p; p++)	/* move to end */
	    ;
	sprintf(p, ":%hu", ntohs(sa->rxsa_in_port));
	return dst;
#ifdef HAVE_IPV6
    case AF_INET6:
	return NULL;
#endif
    default:
	return NULL;
    }
}

/**
 * Return a hash table index for the sockaddr.
 *
 * \param[in]  a      sockaddres to be hashed
 * \param[in]  size   size of the hash table
 */
int
rx_hash_sockaddr(struct rx_sockaddr *sa, int size)
{
    switch (sa->addrtype) {
    case AF_INET:
	/* Original rx peer hash function. */
	return ((sa->rxsa_in_addr ^ sa->rxsa_in_port) % size);
#ifdef HAVE_IPV6
    case AF_INET6:
	return 0;
#endif
    default:
	return 0;
    }
}

/**
 * Compare rx sockaddr a and b. Return true if they are the same.
 *
 * \param[in] a  sockaddr to compare
 * \param[in] b  sockaddr to compare
 * \param[in] mask  bit map of fields to compare
 */
rx_bool_t
rx_compare_sockaddr(struct rx_sockaddr * a, struct rx_sockaddr * b, int mask)
{
    if (a->addrtype != AF_INET || b->addrtype != AF_INET) {
	return 0;		/* AF_INET6 is not supported yet. */
    }
    if ((mask & RXA_ADDR) && (a->rxsa_in_addr != b->rxsa_in_addr)) {
	return 0;
    }
    if ((mask & RXA_PORT) && (a->rxsa_in_port != b->rxsa_in_port)) {
	return 0;
    }
    if ((mask & RXA_SERVICE) && (a->service != b->service)) {
	return 0;
    }
    if ((mask & RXA_SOCKTYPE) && (a->socktype != b->socktype)) {
	return 0;
    }
    return 1;
}

/**
 * Return true if the sockaddr has a loopback address.
 *
 * \param[in] a sockaddr to check
 */
int
rx_is_loopback_sockaddr(struct rx_sockaddr *sa)
{
    switch (sa->addrtype) {
    case AF_INET:
	return rxi_is_loopback_ipv4(ntohl(sa->rxsa_in_addr));
#ifdef HAVE_IPV6
    case AF_INET6:
	return EAFNOSUPPORT;
#endif
    default:
	return EAFNOSUPPORT;
    }
}

/**
 * Copy a sockaddr.
 *
 * \param[in] src source address
 * \param[out] dst destination address
 */
int
rx_copy_sockaddr(struct rx_sockaddr *src, struct rx_sockaddr *dst)
{
    switch (src->addrtype) {
    case AF_INET:
	memset(dst, 0, sizeof(*dst));
	dst->service = src->service;
	dst->socktype = src->socktype;
	dst->addrlen = sizeof(struct sockaddr_in);
	dst->addrtype = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	dst->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
	dst->rxsa_in_family = AF_INET;
	dst->rxsa_in_addr = src->rxsa_in_addr;
	dst->rxsa_in_port = src->rxsa_in_port;
	return 0;
#ifdef HAVE_IPV6
    case AF_INET6:
	return EAFNOSUPPORT;
#endif
    default:
	return EAFNOSUPPORT;
    }
}

#ifndef KERNEL
/**
 * Convert an addrinfo from getaddrinfo() to an rx_sockaddr.
 *
 * \param[in] ai      addrinfo returned from getaddrinfo()
 * \param[in] service rx service id
 * \param[out] sa     rx sockaddr
 */
int
rx_addrinfo_to_sockaddr(struct addrinfo *ai, rx_service_t service,
			struct rx_sockaddr *sa)
{
    switch (ai->ai_family) {
    case AF_INET:
	if (ai->ai_socktype != SOCK_DGRAM) {
	    return EAFNOSUPPORT;
	}
	sa->service = service;
	sa->socktype = SOCK_DGRAM;
	sa->addrlen = ai->ai_addrlen;
	sa->addrtype = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in_len = ai->ai_addrlen;
#endif
	sa->rxsa_in_family = AF_INET;
	sa->rxsa_in_addr =
	    ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr;
	sa->rxsa_in_port = ((struct sockaddr_in *)ai->ai_addr)->sin_port;
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	return EAFNOSUPPORT;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}
#endif

/**
 * Create an rx sockaddr from an generic address.
 *
 * \param[in] a       address to convert
 * \param[in] port    port number
 * \param[in] service rx service number
 * \param[out] sa     rx sockaddr
 */
int
rx_address_to_sockaddr(struct rx_address *a, rx_port_t port,
		       rx_service_t service, struct rx_sockaddr *sa)
{
    switch (a->addrtype) {
    case AF_INET:
	sa->service = service;
	sa->socktype = SOCK_DGRAM;
	sa->addrlen = sizeof(struct sockaddr_in);
	sa->addrtype = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
	sa->rxsa_in_family = AF_INET;
	sa->rxsa_in_addr = htonl(a->rxa_s_addr);
	sa->rxsa_in_port = port;	/* network byte order */
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	return EAFNOSUPPORT;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}

/**
 * Create an generic address from an rx_sockaddr.
 *
 * \param[in]   sa  rx sockaddr
 * \param[out]  a   opaque address
 */
int
rx_sockaddr_to_address(struct rx_sockaddr *sa, struct rx_address *a)
{
    switch (sa->addrtype) {
    case AF_INET:
	a->addrtype = AF_INET;
	a->rxa_s_addr = ntohl(sa->rxsa_in_addr);
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	a->addrtype = AF_INET6;
	memcpy(&a->rxa_s6_addr, &sa->rxsa_in6_addr, sizeof(a->rxa_s6_addr));
	break;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}

/**
 * Convert an IPv4 address and port into an rx_sockaddr.
 *
 * \param[in] ipv4  IPv4 address (network byte order)
 * \param[in] port  port number (network byte order)
 * \param[in] service rx service id
 * \param[out] sa address of an rx_sockaddr to fill
 */
void
rx_ipv4_to_sockaddr(rx_in_addr_t ipv4, rx_port_t port,
		    rx_service_t service, struct rx_sockaddr *sa)
{
    sa->service = service;
    sa->socktype = SOCK_DGRAM;
    sa->addrlen = sizeof(struct sockaddr_in);
    sa->addrtype = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
    sa->rxsa_in_family = AF_INET;
    sa->rxsa_in_addr = ipv4;	/* network byte order */
    sa->rxsa_in_port = port;	/* network byte order */
}

/**
 * Try to get the raw IPv4 address from an rx_sockaddr
 *
 * Returns true and sets ipv4 to the value of the address
 * in network byte order, if the sockaddr contains
 * an IPv4 address. Returns false otherwise.
 *
 * \note: This function should only be used in cases where
 *        a legacy interface requires an IPv4 value.
 *
 * \param[in]  a     sockaddr to check for an IPv4 address
 * \param[out] ipv4  set if a contains an IPv4 address
 */
rx_bool_t
rx_try_sockaddr_to_ipv4(struct rx_sockaddr *a, rx_in_addr_t * ipv4)
{
    if (a->addrtype == AF_INET) {
	*ipv4 = a->rxsa_in_addr;
	return 1;
    }
    return 0;			/* not an ipv4 address */
}


/**
 * Create an opaque address from a raw IPv4 address.
 *
 * \param[in] ipv4  IPv4 address in network byte order
 * \param[out] a  address populated
 */
int
rx_ipv4_to_address(rx_in_addr_t ipv4, struct rx_address *a)
{
    a->addrtype = AF_INET;
    a->rxa_s_addr = ntohl(ipv4);
    return 0;
}

/**
 * Try to get the raw IPv4 address from an opaque address.
 *
 * Returns true and sets ipv4 to the value of the address
 * in network byte order, if the opaque address contains
 * an IPv4 address. Returns false otherwise.
 *
 * \note: This function should only be used in cases where
 *        a legacy interface requires an IPv4 value.
 *
 * \param[in]  a     address to check for an IPv4 address
 * \param[out] ipv4  set if a contains an IPv4 address
 */
rx_bool_t
rx_try_address_to_ipv4(struct rx_address * a, rx_in_addr_t * ipv4)
{
    if (a->addrtype == AF_INET) {
	*ipv4 = a->rxa_s_addr;
	return 1;
    }
    return 0;			/* not an ipv4 address */
}

/**
 * Print the address to a buffer as a readable string.
 *
 * \param[in]  a     address to be printed
 * \param[out] dst   buffer to be filled
 * \param[in]  size  number of bytes available in dst
 */
char *
rx_print_address(struct rx_address *a, char *dst, size_t size)
{
    switch (a->addrtype) {
    case AF_INET: {
	struct in_addr addr;
	addr.s_addr = htonl(a->rxa_s_addr);
	return rxi_inet_ntop_v4(&addr, dst, size);
    }
#ifdef HAVE_IPV6
    case AF_INET6:
	return NULL;
#endif
    default:
	return NULL;
    }
}

/**
 * Compare address a and b. Return true if they are the same.
 *
 * \param[in] a  address to compare
 * \param[in] b  address to compare
 */
rx_bool_t
rx_compare_address(struct rx_address * a, struct rx_address * b)
{
    if (a->addrtype != AF_INET || b->addrtype != AF_INET) {
	return 0;		/* AF_INET6 is not supported yet. */
    }
    return a->rxa_s_addr == a->rxa_s_addr;
}

/**
 * Return true if address is a loopback address.
 *
 * \param[in] a address to check
 */
int
rx_is_loopback_address(struct rx_address *a)
{
    switch (a->addrtype) {
    case AF_INET:
	return rxi_is_loopback_ipv4(a->rxa_s_addr);
#ifdef HAVE_IPV6
    case AF_INET6:
	return EAFNOSUPPORT;
#endif
    default:
	return EAFNOSUPPORT;
    }
}

/**
 * Create a copy of an address.
 *
 * \param[in] src source address
 * \param[out] dst destination address
 */
int
rx_copy_address(struct rx_address *src, struct rx_address *dst)
{
    dst->addrtype = src->addrtype;
    switch (src->addrtype) {
    case AF_INET:
	dst->rxa_s_addr = src->rxa_s_addr;
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	memcpy(&dst->rxa_s6_addr, &src->rxa_s6_addr, sizeof(dst->rxa_s6_addr));
	break;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}