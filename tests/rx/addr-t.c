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

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <pthread.h>

#include <tests/tap/basic.h>

#include <rx/rx_addr.h>

/* helper */
inline int
octet(afs_uint32 addr, int n)
{
    return (int)((addr >> (8 * n)) & 0xff);
}

/* test helper functions */
rx_in_addr_t
make_in_addr(const char *text)
{
    rx_in_addr_t addr;
    struct in_addr in;

    inet_pton(AF_INET, text, &in);
    memcpy(&addr, &(in.s_addr), sizeof(addr));
    return addr;
}

/* Create an client rx_sockaddr; suitable for rx_NewConnection2() */
void
make_sockaddr(const char *address, struct rx_sockaddr *sa)
{
    int code;
    const char *service = "1234";	/* test port number */
    struct addrinfo hints;
    struct addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    /* family is determined by the format of 'address' */
    hints.ai_flags = AI_NUMERICHOST;	/* conversion only; no dns lookup */
    hints.ai_socktype = SOCK_DGRAM;

    code = getaddrinfo(address, service, &hints, &results);
    if (code) {
	bail("getaddrinfo: %s (%d)", gai_strerror(code), code);
    }
    if (!results) {
	bail("getaddrinfo: did not return a result\n");
    }

    /* use first result */
    code = rx_addrinfo_to_sockaddr(results, 0, sa);
    if (code) {
	bail("rx_addrinfo_to_sockaddr: %d", code);
    }

    freeaddrinfo(results);
}

/* Create server INADDR_ANY (IPv4) rx_sockaddr; suitable for rx_Init2() */
void
make_any_sockaddr(struct rx_sockaddr *sa)
{
    int code;
    const char *service = "1234";	/* test port number */
    struct addrinfo hints;
    struct addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    hints.ai_socktype = SOCK_DGRAM;

    code = getaddrinfo(NULL, service, &hints, &results);
    if (code) {
	bail("getaddrinfo: %s (%d)", gai_strerror(code), code);
    }
    if (!results) {
	bail("getaddrinfo: did not return a result\n");
    }

    /* use first result */
    code = rx_addrinfo_to_sockaddr(results, 0, sa);
    if (code) {
	bail("rx_addrinfo_to_sockaddr: %d", code);
    }

    freeaddrinfo(results);
}

/* example usage */
int
example_legacy_api(afs_uint32 addr, char dst[16])
{
    addr = ntohl(addr);
    sprintf(dst, "%d.%d.%d.%d",
	    octet(addr, 3), octet(addr, 2), octet(addr, 1), octet(addr, 0));
    return 0;
}

/* example usage */
int
example_try_sockaddr_to_ipv4(struct rx_sockaddr *sa)
{
    int code;
    rx_in_addr_t ipv4;
    char buffer[16];

    memset(buffer, 0, sizeof(buffer));
    if (rx_try_sockaddr_to_ipv4(sa, &ipv4)) {
	code = example_legacy_api(ipv4, buffer);
    } else {
	/* Not an IPv4 address: Need a new API, or return unsupported. */
	code = EAFNOSUPPORT;
    }
    is_string("1.2.3.4", buffer, "example_try_sockaddr_to_ipv4");
    return code;
}

int
main(void)
{
    char buf[64];
    int hashsize = 257;
    rx_in_addr_t a4;
    struct rx_address *a;
    struct rx_sockaddr *sa;
    struct rx_sockaddr *sa2;

    a = malloc(sizeof(*a));
    sa = malloc(sizeof(*sa));
    sa2 = malloc(sizeof(*sa2));
    memset(buf, 0, sizeof(buf));

    plan(11);

    make_sockaddr("1.2.3.4", sa);
    is_string("1.2.3.4:1234", rx_print_sockaddr(sa, buf, sizeof(buf)),
	      "rx_addrinfo_to_sockaddr");

    /* printf("hash %s -> %d\n", rx_print_sockaddr(sa, buf, sizeof(buf)),
     * rx_hash_sockaddr(sa, hashsize)); */
    ok(rx_hash_sockaddr(sa, hashsize) == 53, "rx_hash_sockaddr");
    ok(!rx_is_loopback_sockaddr(sa), "rx_is_loopback");

    make_any_sockaddr(sa);
    is_string("0.0.0.0:1234", rx_print_sockaddr(sa, buf, sizeof(buf)),
	      "rx_addrinfo_to_sockaddr any");

    make_sockaddr("1.2.3.4", sa);
    example_try_sockaddr_to_ipv4(sa);

    ok(rx_copy_sockaddr(sa, sa2) == 0, "rx_copy_sockaddr");
    is_string("1.2.3.4:1234", rx_print_sockaddr(sa, buf, sizeof(buf)),
	      "rx_copy_sockaddr: contents");

    ok(rx_compare_sockaddr(sa, sa2), "rx_compare_sockaddr: same");

    make_sockaddr("9.9.9.9", sa);
    ok(!rx_compare_sockaddr(sa, sa2), "rx_compare_sockaddr: diff");

    /* address -> ipv4 */
    a4 = make_in_addr("1.2.3.4");
    rx_ipv4_to_address(a4, a);
    ok(rx_address_to_sockaddr(a, htons(1234), 1, sa) == 0,
       "rx_address_to_sockaddr");
    is_string("1.2.3.4:1234", rx_print_sockaddr(sa, buf, sizeof(buf)),
	      "test rx_address_to_sockaddr");
    rx_free_address(a);
    memset(buf, 0, sizeof(buf));

    free(a);
    free(sa);
    free(sa2);
    return 0;
}
