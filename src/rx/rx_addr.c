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

#ifndef KERNEL
#include <roken.h>
#else
#include "afs/sysincludes.h"
#include "afsincludes.h"
#endif

#include <rx/rx.h>
#include <rx/rx_addr.h>

#define PORTSTRLEN 6 /* max port number strlen plus null */

#ifndef KERNEL

static_inline char *
inet_ntop_v4(const void *src, char *dst, size_t size)
{
    return (char *)inet_ntop(AF_INET, src, dst, size);
}

static_inline char *
inet_ntop_v6(const void *src, char *dst, size_t size)
{
    return (char *)inet_ntop(AF_INET6, src, dst, size);
}

#else /* KERNEL */

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN  16
#endif

/* inet_ntop for ipv4 */
/* Adapted from Heimdal libroken. */
static char *
inet_ntop_v4(void *src, char *dst, size_t size)
{
    const char digits[] = "0123456789";
    int i;
    struct in_addr *addr = (struct in_addr *)src;
    u_long a = ntohl(addr->s_addr);
    char *orig_dst = dst;

    if (size < INET_ADDRSTRLEN) {
	return NULL; /* ENOSPC */;
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

/* inet_ntop for ipv6 */
/* Adapted from Heimdal libroken. */
static char *
inet_ntop_v6(const void *src, char *dst, size_t size)
{
    const char xdigits[] = "0123456789abcdef";
    int i;
    const struct in6_addr *addr = (struct in6_addr *)src;
    const u_char *ptr = addr->s6_addr;
    char *orig_dst = dst;
    int compressed = 0;

    if (size < INET6_ADDRSTRLEN) {
	return NULL; /* ENOSPC */
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
#endif /* HAVE_IPV6 */
#endif /* KERNEL */

/**
 * Return true if the ipv4 address is a loopback.
 */
static_inline int
is_loopback_v4(rx_in_addr_t addr)
{
    return ((ntohl(addr) & 0xffff0000) == 0x7f000000);
}


static_inline int
is_loopback_v6(struct in6_addr *addr)
{
    afs_uint32 *a = (afs_uint32 *) addr;
    return (a[0] == 0) && (a[1] == 0) && (a[2] == 0) && (a[3] == htonl(1));
}

static_inline int
is_v4_mapped(struct in6_addr *addr)
{
    afs_uint32 *a = (afs_uint32 *) addr;
    return (a[0] == 0) && (a[1] == 0) && (a[2] == htonl(0xffff));
}

static_inline int
is_v4_compat(struct in6_addr *addr)
{
    afs_uint32 *a = (afs_uint32 *) addr;
    return (a[0] == 0) && (a[1] == 0) && (a[2] == 0) && (a[3] > 1);
}

static_inline int
is_equal_v6(struct in6_addr *x, struct in6_addr *y)
{
    afs_uint32 *a = (afs_uint32 *) x;
    afs_uint32 *b = (afs_uint32 *) y;
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]) && (a[3] == b[3]);
}

static char *
append_port_str(rx_port_t port, char *sep, size_t seplen, char *dst, size_t size)
{
    size_t len = strlen(dst);
    if (size < len + seplen + PORTSTRLEN) {
	return dst; /* ENOSPC */
    }
    snprintf(dst + len, seplen + PORTSTRLEN, "%s%hu", sep, ntohs(port));
    return dst;
}

static int
compare_sockaddr(struct rx_sockaddr *a, struct rx_sockaddr *b)
{
    if (a->rxsa_family == AF_INET && b->rxsa_family == AF_INET) {
	return a->rxsa_s_addr == b->rxsa_s_addr;
    }
#ifdef HAVE_IPV6
    if (a->rxsa_family == AF_INET6 && b->rxsa_family == AF_INET6) {
	return is_equal_v6(&a->rxsa_in6_addr, &b->rxsa_in6_addr);
    }
    if (a->rxsa_family == AF_INET6 && b->rxsa_family == AF_INET) {
	rx_in_addr_t ipv4;
	if (rx_try_sockaddr_to_ipv4(a, &ipv4)) {
	    return ipv4 == b->rxsa_s_addr;
	} else {
	    return 0;
	}
    }
    if (a->rxsa_family == AF_INET && b->rxsa_family == AF_INET6) {
	rx_in_addr_t ipv4;
	if (rx_try_sockaddr_to_ipv4(b, &ipv4)) {
	    return a->rxsa_s_addr == ipv4;
	} else {
	    return 0;
	}
    }
#endif
    return 0;
}

/**
 * Print the address and port to a buffer as a readable string.
 *
 * @param[in]  a     address to be printed
 * @param[out] dst   buffer to be filled
 * @param[in]  size  number of bytes available in dst
 */
char *
rx_print_sockaddr(struct rx_sockaddr *sa, char *dst, size_t size)
{
    switch (sa->rxsa_family) {
    case AF_INET:
	if (size < INET_ADDRSTRLEN + PORTSTRLEN) {
	    dst[0] = '\0';
	} else {
	    inet_ntop_v4(&sa->rxsa_in_addr, dst, size);
	    append_port_str(sa->rxsa_in_port, ":", 1, dst, size);
	}
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	if (size < INET6_ADDRSTRLEN + PORTSTRLEN + 2) {
	    dst[0] = '\0';
	} else {
	    dst[0] = '[';
	    inet_ntop_v6(&sa->rxsa_in6_addr, dst+1, size - 1);
	    append_port_str(sa->rxsa_in6_port, "]:", 2, dst, size - 1);
	}
	break;
#endif
    default:
	dst[0] = '\0'; /* EAFNOSUPPORT */
    }
    return dst;
}

/**
 * Return a hash table index for the sockaddr.
 *
 * @param[in]  a      sockaddres to be hashed
 * @param[in]  hashsize   size of the hash table
 */
int
rx_hash_sockaddr(struct rx_sockaddr *sa, int hashsize)
{
    switch (sa->rxsa_family) {
    case AF_INET:
	/* Original rx peer hash function. */
	return ((sa->rxsa_s_addr ^ sa->rxsa_in_port) % hashsize);
#ifdef HAVE_IPV6
    case AF_INET6: {
	afs_uint32 p = (afs_uint32)sa->rxsa_in6_port;
	afs_uint32 *a = (afs_uint32 *)&sa->rxsa_in6_addr;
	afs_uint32 xor;

	if (is_v4_mapped(&sa->rxsa_in6_addr) || is_v4_compat(&sa->rxsa_in6_addr)) {
	    xor = a[3] ^ p;
	} else {
	    xor = a[0] ^ a[1] ^ a[2] ^ a[3] ^ p;
	}
	return xor % hashsize;
    }
#endif
    default:
	return 0;
    }
}

/**
 * Compare rx sockaddr a and b. Return true if they are the same.
 *
 * @param[in] a  sockaddr to compare
 * @param[in] b  sockaddr to compare
 * @param[in] mask  bit map of fields to compare
 */
rx_bool_t
rx_compare_sockaddr(struct rx_sockaddr * a, struct rx_sockaddr * b, int mask)
{
    if ((mask & RXA_ADDR) && (compare_sockaddr(a, b) == 0)) {
	return 0;
    }
    if ((mask & RXA_PORT) && (rx_get_sockaddr_port(a) != rx_get_sockaddr_port(b))) {
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
 * @param[in] a sockaddr to check
 */
int
rx_is_loopback_sockaddr(struct rx_sockaddr *sa)
{
    switch (sa->rxsa_family) {
    case AF_INET:
	return is_loopback_v4(sa->rxsa_s_addr);
#ifdef HAVE_IPV6
    case AF_INET6:
	return is_loopback_v6(&sa->rxsa_in6_addr);
#endif
    default:
	return 0; /* EAFNOSUPPORT */
    }
}

/**
 * Copy a sockaddr.
 *
 * @param[in] src source address
 * @param[out] dst destination address
 */
int
rx_copy_sockaddr(struct rx_sockaddr *src, struct rx_sockaddr *dst)
{
    switch (src->rxsa_family) {
    case AF_INET:
	memset(dst, 0, sizeof(*dst));
	dst->service = src->service;
	dst->socktype = src->socktype;
	dst->addrlen = sizeof(struct sockaddr_in);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	dst->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
	dst->rxsa_in_family = AF_INET;
	dst->rxsa_in_addr = src->rxsa_in_addr;
	dst->rxsa_in_port = src->rxsa_in_port;
	return 0;
#ifdef HAVE_IPV6
    case AF_INET6:
	memset(dst, 0, sizeof(*dst));
	dst->service = src->service;
	dst->socktype = src->socktype;
	dst->addrlen = sizeof(struct sockaddr_in6);
#ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
	dst->rxsa_in6_len = sizeof(struct sockaddr_in6);
#endif
	dst->rxsa_in6_family = AF_INET6;
	dst->rxsa_in6_addr = src->rxsa_in6_addr;
	dst->rxsa_in6_port = src->rxsa_in6_port;
	return 0;
#endif
    default:
	return EAFNOSUPPORT;
    }
}

#ifndef KERNEL
/**
 * Convert an addrinfo from getaddrinfo() to an rx_sockaddr.
 *
 * @param[in] ai      addrinfo returned from getaddrinfo()
 * @param[in] service rx service id
 * @param[out] sa     rx sockaddr
 */
int
rx_addrinfo_to_sockaddr(struct addrinfo *ai, rx_service_t service,
			struct rx_sockaddr *sa)
{
    switch (ai->ai_family) {
    case AF_INET:
	sa->service = service;
	sa->socktype = ai->ai_socktype; /* SOCK_DGRAM */
	sa->addrlen = sizeof(struct sockaddr_in);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
	sa->rxsa_in_family = AF_INET;
	sa->rxsa_in_addr =
	    ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
	sa->rxsa_in_port =
	    ((struct sockaddr_in *)ai->ai_addr)->sin_port;
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	sa->service = service;
	sa->socktype = ai->ai_socktype; /* SOCK_DGRAM */
	sa->addrlen = sizeof(struct sockaddr_in6);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in6_len = sizeof(struct sockaddr_in6);
#endif
	sa->rxsa_in6_family = AF_INET6;
	sa->rxsa_in6_addr =
	    ((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
	sa->rxsa_in6_port =
	    ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
	break;
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
 * @param[in] a       address to convert
 * @param[in] port    port number
 * @param[in] service rx service number
 * @param[out] sa     rx sockaddr
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
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
	sa->rxsa_in_family = AF_INET;
	sa->rxsa_s_addr = a->rxa_s_addr; /* network byte order */
	sa->rxsa_in_port = port;	/* network byte order */
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	sa->service = service;
	sa->socktype = SOCK_DGRAM;
	sa->addrlen = sizeof(struct sockaddr_in6);
#ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
	sa->rxsa_in6_len = sizeof(struct sockaddr_in6);
#endif
	sa->rxsa_in6_family = AF_INET6;
	sa->rxsa_in6_addr = a->rxa_in6_addr; /* network byte order */
	sa->rxsa_in6_port = port;	/* network byte order */
	break;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}

/**
 * Create an generic address from an rx_sockaddr.
 *
 * @param[in]   sa  rx sockaddr
 * @param[out]  a   opaque address
 */
int
rx_sockaddr_to_address(struct rx_sockaddr *sa, struct rx_address *a)
{
    switch (sa->rxsa_family) {
    case AF_INET:
	a->addrtype = AF_INET;
	a->rxa_s_addr = sa->rxsa_s_addr; /* network byte order */
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	a->addrtype = AF_INET6;
	a->rxa_in6_addr = sa->rxsa_in6_addr; /* network byte order */
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
 * @param[in] ipv4  IPv4 address (network byte order)
 * @param[in] port  port number (network byte order)
 * @param[in] service rx service id
 * @param[out] sa address of an rx_sockaddr to fill
 */
void
rx_ipv4_to_sockaddr(rx_in_addr_t ipv4, rx_port_t port,
		    rx_service_t service, struct rx_sockaddr *sa)
{
    sa->service = service;
    sa->socktype = SOCK_DGRAM;
    sa->addrlen = sizeof(struct sockaddr_in);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    sa->rxsa_in_len = sizeof(struct sockaddr_in);
#endif
    sa->rxsa_in_family = AF_INET;
    sa->rxsa_s_addr = ipv4;	/* network byte order */
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
 * @param[in]  a     sockaddr to check for an IPv4 address
 * @param[out] ipv4  set if a contains an IPv4 address
 */
rx_bool_t
rx_try_sockaddr_to_ipv4(struct rx_sockaddr *a, rx_in_addr_t * ipv4)
{
    switch (a->rxsa_family) {
    case AF_INET:
	*ipv4 = a->rxsa_s_addr;
	return 1;
#ifdef HAVE_IPV6
    case AF_INET6:
	if (is_v4_mapped(&a->rxsa_in6_addr) || is_v4_compat(&a->rxsa_in6_addr)) {
	    *ipv4 = ((rx_in_addr_t*)(&a->rxsa_in6_addr))[3];
	    return 1;
	}
	break;
#endif
    default:
	break;
    }
    return 0;			/* not an ipv4 address */
}

/**
 * Create an opaque address from a raw IPv4 address.
 *
 * @param[in] ipv4  IPv4 address in network byte order
 * @param[out] a  address populated
 */
void
rx_ipv4_to_address(rx_in_addr_t ipv4, struct rx_address *a)
{
    a->addrtype = AF_INET;
    a->rxa_s_addr = ipv4;
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
 * @param[in]  a     address to check for an IPv4 address
 * @param[out] ipv4  set if a contains an IPv4 address
 */
rx_bool_t
rx_try_address_to_ipv4(struct rx_address *a, rx_in_addr_t *ipv4)
{
    switch (a->addrtype) {
    case AF_INET:
	*ipv4 = a->rxa_s_addr;  /* network byte order */
	return 1;
#ifdef HAVE_IPV6
    case AF_INET6:
	if (is_v4_mapped(&a->rxa_in6_addr) || is_v4_compat(&a->rxa_in6_addr)) {
	    *ipv4 = ((rx_in_addr_t*)(&a->rxa_in6_addr))[3];
	    return 1;
	}
	break;
#endif
    default:
	break;
    }
    return 0;			/* not an ipv4 address */
}

/**
 * Print the address to a buffer as a readable string.
 *
 * @param[in]  a     address to be printed
 * @param[out] dst   buffer to be filled
 * @param[in]  size  number of bytes available in dst
 */
char *
rx_print_address(struct rx_address *a, char *dst, size_t size)
{
    switch (a->addrtype) {
    case AF_INET: {
	inet_ntop_v4(&a->rxa_in_addr, dst, size);
	break;
    }
#ifdef HAVE_IPV6
    case AF_INET6:
	if (size < INET6_ADDRSTRLEN + PORTSTRLEN + 2) {
	    dst[0] = '\0';
	} else {
	    inet_ntop_v6(&a->rxa_in6_addr, dst, size);
	}
	break;
#endif
    default:
	dst[0] = '\0'; /* EAFNOSUPPORT */
    }
    return dst;
}

/**
 * Compare address a and b. Return true if they are the same.
 *
 * @param[in] a  address to compare
 * @param[in] b  address to compare
 */
rx_bool_t
rx_compare_address(struct rx_address *a, struct rx_address *b)
{
    if (a->addrtype == AF_INET && b->addrtype == AF_INET) {
	return a->rxa_s_addr == b->rxa_s_addr;
    }
#ifdef HAVE_IPV6
    if (a->addrtype == AF_INET6 && b->addrtype == AF_INET6) {
	return is_equal_v6(&a->rxa_in6_addr, &b->rxa_in6_addr);
    }
    if (a->addrtype == AF_INET6 && b->addrtype == AF_INET) {
	rx_in_addr_t ipv4;
	if (rx_try_address_to_ipv4(a, &ipv4)) {
	    return ipv4 == b->rxa_s_addr;
	} else {
	    return 0;
	}
    }
    if (a->addrtype == AF_INET && b->addrtype == AF_INET6) {
	rx_in_addr_t ipv4;
	if (rx_try_address_to_ipv4(b, &ipv4)) {
	    return a->rxa_s_addr == ipv4;
	} else {
	    return 0;
	}
    }
#endif
    return 0;
}

/**
 * Return true if address is a loopback address.
 *
 * @param[in] a address to check
 */
int
rx_is_loopback_address(struct rx_address *a)
{
    switch (a->addrtype) {
    case AF_INET:
	return is_loopback_v4(a->rxa_s_addr);
#ifdef HAVE_IPV6
    case AF_INET6:
	return is_loopback_v6(&a->rxa_in6_addr);
#endif
    default:
	return EAFNOSUPPORT;
    }
}

/**
 * Create a copy of an address.
 *
 * @param[in] src source address
 * @param[out] dst destination address
 */
int
rx_copy_address(struct rx_address *src, struct rx_address *dst)
{
    dst->addrtype = src->addrtype;
    switch (src->addrtype) {
    case AF_INET:
	dst->rxa_in_addr = src->rxa_in_addr;
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	dst->rxa_in6_addr = src->rxa_in6_addr;
	break;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}

int
rx_serialize_address(struct rx_address *src, unsigned char *dst, size_t size)
{
    if (size < 16) {
	return ENOSPC;
    }
    switch (src->addrtype) {
    case AF_INET: {
	    afs_uint32 *a = (afs_uint32 *)dst;
	    a[0] = 0;
	    a[1] = 0;
	    a[2] = htonl(0xffff);
	    a[3] = (afs_uint32)src->rxa_s_addr;
	}
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	memcpy(dst, &src->rxa_in6_addr, sizeof(src->rxa_in6_addr));
	break;
#endif
    default:
	return EAFNOSUPPORT;
    }
    return 0;
}

int
rx_deserialize_address(unsigned char *src, struct rx_address *dst)
{
    afs_uint32 *a = (afs_uint32 *)src;

    if ((a[0] == 0) && (a[1] == 0) && (a[2] == htonl(0xffff))) {
	rx_ipv4_to_address(a[3], dst);
    } else {
#ifdef HAVE_IPV6
	dst->addrtype = AF_INET6;
	memcpy(&dst->rxa_in6_addr, src, sizeof(dst->rxa_in6_addr));
#else
	return EAFNOSUPPORT;
#endif
    }
    return 0;
}

