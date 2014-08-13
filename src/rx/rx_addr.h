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

#define HAVE_IPV6 1 /* todo: configure check */

typedef int rx_bool_t;
typedef in_addr_t rx_in_addr_t;
typedef in_port_t rx_port_t;
typedef u_short rx_service_t;
typedef char rx_addr_str_t[64];	/**< for rx_print_sockaddr, rx_print_address */

/**
 * Generic address (IPv4 or IPv6)
 */
struct rx_address {
    int addrtype;	      /**< type of address: AF_INET */
    union {
	struct in_addr in;	/* host byte order */
#ifdef HAVE_IPV6
	struct in6_addr in6;
#endif
    } addr;
};
#define rxa_s_addr    addr.in.s_addr
#ifdef HAVE_IPV6
# define rxa_s6_addr  addr.in6.s6_addr
#endif

/**
 * Socket address for rx
 */
struct rx_sockaddr {
    rx_service_t service; /**< rx service id */
    int socktype;         /**< socket type (SOCK_DGRAM) */
    socklen_t   addrlen;  /**< addr length */
    sa_family_t addrtype; /**< addr discriminator (AF_INET, AF_INET6) */
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
	struct sockaddr_storage ss;	/* for alignment */
#endif
    } addr;
};
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
# define rxsa_in_len      addr.sin.sin_len
#endif
#define rxsa_in_family    addr.sin.sin_family
#define rxsa_in_addr      addr.sin.sin_addr.s_addr
#define rxsa_in_port      addr.sin.sin_port
#if HAVE_IPV6
# ifdef STRUCT_SOCKADDR_HAS_SA6_LEN
#  define rxsa_in6_len    addr.sin6.sin6_len
# endif
# define rxsa_in6_family  addr.sin6.sin6_family
# define rxsa_in6_addr    addr.sin6.sin6_addr.s6_addr
# define rxsa_in6_port    addr.sin6.sin6_port
#endif

/* Masks for rx_compare_sockaddr_m() */
#define RXA_ADDR      0x01
#define RXA_PORT      0x02
#define RXA_SERVICE   0x04
#define RXA_SOCKTYPE  0x08
#define RXA_AP        (RXA_ADDR | RXA_PORT)
#define RXA_ALL       (RXA_ADDR | RXA_PORT | RXA_SERVICE | RXA_SOCKTYPE)

static_inline rx_port_t
rx_get_sockaddr_port(struct rx_sockaddr *sa)
{
    return sa->addrtype == AF_INET ? sa->rxsa_in_port :
#if HAVE_IVP6
	(sa->addrtype == AF_INET6 ? sa->rxsa_in6_port : 0);
#else
	0;
#endif
}

static_inline rx_port_t
rx_set_sockaddr_port(struct rx_sockaddr * sa, rx_port_t port)
{
    return sa->addrtype == AF_INET ? (sa->rxsa_in_port = port) :
#if HAVE_IVP6
	(sa->addrtype == AF_INET6 ? (sa->rxsa_in6_port = port) : port);
#else
	port;
#endif
}


/* I hate camel case. I really do. I suppose these will need to
   be renamed. Blahh. */

/* rx_sockaddr */
#ifndef KERNEL
int rx_addrinfo_to_sockaddr(struct addrinfo *a, rx_service_t service,
			    struct rx_sockaddr *sa);
#endif
char *rx_print_sockaddr(struct rx_sockaddr *a, char *dst, size_t size);
int rx_hash_sockaddr(struct rx_sockaddr *a, int size);
int rx_compare_sockaddr(struct rx_sockaddr *a, struct rx_sockaddr *b,
			int mask);
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
int rx_address_to_sockaddr(struct rx_address *a, rx_port_t port,
			   rx_service_t service, struct rx_sockaddr *sa);
int rx_sockaddr_to_address(struct rx_sockaddr *sa, struct rx_address *a);

/* For compability with IPv4-only interfaces. */
int rx_ipv4_to_address(rx_in_addr_t ipv4, struct rx_address *a);
rx_bool_t rx_try_address_to_ipv4(struct rx_address *a, rx_in_addr_t * ipv4);

#endif /* OPENAFS_RX_ADDR_H */
