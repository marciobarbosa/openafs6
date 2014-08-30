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

#include <tests/tap/basic.h>
#include <rx/rx_addr.h>

/* help: create ipv4 address */
rx_in_addr_t
make_in_addr(const char *text)
{
    rx_in_addr_t addr;
    struct in_addr in;

    inet_pton(AF_INET, text, &in);
    memcpy(&addr, &(in.s_addr), sizeof(addr));
    return addr;
}

/* helper: create an client rx_sockaddr */
void
make_sockaddr(const char *address, const char *port, struct rx_sockaddr *sa)
{
    int code;
    struct addrinfo hints;
    struct addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    /* family is determined by the format of 'address' */
    hints.ai_flags = AI_NUMERICHOST;	/* conversion only; no dns lookup */
    hints.ai_socktype = SOCK_DGRAM;

    code = getaddrinfo(address, port, &hints, &results);
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

/* helper: create an client rx_address */
void
make_address(const char *address, struct rx_address *a)
{
    struct rx_sockaddr sa;
    make_sockaddr(address, "0", &sa);
    rx_sockaddr_to_address(&sa, a);
}

/* helper: create server INADDR_ANY rx_sockaddr */
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

/* helper: get an address octet */
static_inline int
octet(afs_uint32 a, int n)
{
    return (a >> (8 * n)) & 0xff;
}

char*
print_ipv4(rx_in_addr_t addr, char dst[16])
{
    int a = ntohl(addr);
    sprintf(dst, "%d.%d.%d.%d", octet(a, 3), octet(a, 2), octet(a, 1), octet(a, 0));
    return dst;
}

void
test_sockaddr_print(void)
{
    struct rx_sockaddr sa;
    char buf[64];

    make_sockaddr("1.2.3.4", "1234", &sa);
    is_string("", rx_print_sockaddr(&sa, buf, 13), "print buf too small");
    is_string("1.2.3.4:1234", rx_print_sockaddr(&sa, buf, sizeof(buf)),
	      "print sockaddr v4");

    make_sockaddr("1:2:3:4:5:6:7:8", "1234", &sa);
    is_string("[1:2:3:4:5:6:7:8]:1234",
	      rx_print_sockaddr(&sa, buf, sizeof(buf)), "print sockaddr v6");
}

void
test_sockaddr_port(void)
{
    struct rx_sockaddr sa;

    make_sockaddr("1.2.3.4", "12345", &sa);
    is_int(12345, ntohs(rx_get_sockaddr_port(&sa)), "get port v4");
    is_int(9999, ntohs(rx_set_sockaddr_port(&sa, htons(9999))), "set port return value v4");
    is_int(9999, ntohs(rx_get_sockaddr_port(&sa)), "set port v4");

    make_sockaddr("1:2:3:4:5:6:7:8", "12345", &sa);
    is_int(12345, ntohs(rx_get_sockaddr_port(&sa)), "get port v6");
    is_int(9999, ntohs(rx_set_sockaddr_port(&sa, htons(9999))), "set port return value v6");
    is_int(9999, ntohs(rx_get_sockaddr_port(&sa)), "set port v6");
}


struct hash_test_case {
    const char *addr;
    const char *port;
    int hashsize;
    int expect;
} hash_test_cases[] = {
    {"1.2.3.4", "1234", 257, 53},
    {"1:2:3:4:5:6:7:8", "1234", 257, 43},
    {"18.52.171.205", "1234", 257, 15},
    {"0:0:0:0:0:0:1234:abcd", "1234", 257, 15},
    {"0:0:0:0:0:ffff:1234:abcd", "1234", 257, 15},
    {NULL}
};

void
test_sockaddr_hash(void)
{
    struct hash_test_case *tc;
    struct rx_sockaddr sa;

    for (tc = hash_test_cases; tc->addr; tc++) {
        make_sockaddr(tc->addr, tc->port, &sa);
        is_int(tc->expect, rx_hash_sockaddr(&sa, tc->hashsize),
	     "hash [%s]:%s", tc->addr, tc->port);
    }
}

void
test_sockaddr_copy(void)
{
    struct rx_sockaddr a;
    struct rx_sockaddr b;
    char buf[64];

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    make_sockaddr("1.2.3.4", "1234", &a);
    ok(rx_copy_sockaddr(&a, &b) == 0, "copy sockaddr return");
    is_string("1.2.3.4:1234", rx_print_sockaddr(&b, buf, sizeof(buf)),
        "copied sockaddr contents are ok v4");

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    make_sockaddr("1:2:3:4:5:6:7:8", "1234", &a);
    ok(rx_copy_sockaddr(&a, &b) == 0, "copy sockaddr return");
    is_string("[1:2:3:4:5:6:7:8]:1234", rx_print_sockaddr(&b, buf, sizeof(buf)),
        "copied sockaddr contents are ok v6");

}


struct to_ipv4_test_case {
    const char *addr;
    const char *port;
    const char *expect;
} to_ipv4_test_cases[] = {
    {"1.2.3.4", "1234", "1.2.3.4"},
    {"1:2:3:4:5:6:7:8", "1234", NULL},
    {"0:0:0:0:0:0:1234:abcd", "1234", "18.52.171.205"},
    {"0:0:0:0:0:ffff:1234:abcd", "1234", "18.52.171.205"},
    {NULL}
};

void
test_sockaddr_to_ipv4(void)
{
    struct to_ipv4_test_case *tc;
    struct rx_sockaddr a;
    int result;
    rx_in_addr_t got;
    char buf[16];

    for (tc = to_ipv4_test_cases; tc->addr; tc++) {
	make_sockaddr(tc->addr, tc->port, &a);
	result = rx_try_sockaddr_to_ipv4(&a, &got);
	if (!tc->expect) {
	    ok(result == 0, "try sockaddr to ipv4: %s is not ipv4", tc->addr);
	} else {
	    ok(result, "try sockaddr to ipv4: %s is ipv4", tc->addr);
	    if (result) {
		is_string(tc->expect, print_ipv4(got, buf),
			  "try sockaddr to ipv4: %s -> %s",
			  tc->addr, tc->expect);
	    }
	}
    }
}

void
test_address_to_ipv4(void)
{
    struct to_ipv4_test_case *tc;
    struct rx_address a;
    int result;
    rx_in_addr_t got;
    char buf[16];

    for (tc = to_ipv4_test_cases; tc->addr; tc++) {
	make_address(tc->addr, &a);
	result = rx_try_address_to_ipv4(&a, &got);
	if (!tc->expect) {
	    ok(result == 0, "try address to ipv4: %s is not ipv4", tc->addr);
	} else {
	    ok(result, "try address to ipv4: %s is ipv4", tc->addr);
	    if (result) {
		is_string(tc->expect, print_ipv4(got, buf),
			  "try address to ipv4: %s -> %s",
			  tc->addr, tc->expect);
	    }
	}
    }
}

struct loopback_test_case {
    const char *addr;
    const char *port;
    int expect;
} loopback_test_cases[] = {
    {"1.2.3.4", "1234", 0},
    {"127.0.0.0", "1234", 1}, /* should this be a loopback? */
    {"127.0.0.1", "1234", 1},
    {"127.0.1.1", "1234", 1},
    {"0:0:0:0:0:0:0:1", "1234", 1},
    {"1:2:3:4:5:6:7:8", "1234", 0},
    {NULL}
};

void
test_sockaddr_loopback(void)
{
    struct loopback_test_case *tc;
    struct rx_sockaddr a;

    for (tc = loopback_test_cases; tc->addr; tc++) {
	make_sockaddr(tc->addr, tc->port, &a);
	ok(rx_is_loopback_sockaddr(&a) == tc->expect,
	   "sockaddr loopback: %s, %s loopback",
	    tc->addr, tc->expect ? "is" : "not");
    }
}

void
test_address_loopback(void)
{
    struct loopback_test_case *tc;
    struct rx_address a;

    for (tc = loopback_test_cases; tc->addr; tc++) {
	make_address(tc->addr, &a);
	ok(rx_is_loopback_address(&a) == tc->expect,
	   "address loopback: %s %s loopback",
	    tc->addr, tc->expect ? "is" : "is not");
    }
}

struct sockaddr_compare_test_case {
    struct {
        const char *addr;
        const char *port;
    } a;
    struct {
        const char *addr;
        const char *port;
    } b;
    int mask;
    int expect;
} sockaddr_compare_test_cases[] = {
    {{"0.0.0.0", "0"}, {"0.0.0.0", "0"}, RXA_ALL, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "1234"}, RXA_ALL, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "1234"}, RXA_AP, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "1234"}, RXA_ADDR, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "1234"}, RXA_PORT, 1},
    {{"9.9.9.9", "1234"}, {"1.2.3.4", "1234"}, RXA_ALL, 0},
    {{"9.9.9.9", "1234"}, {"1.2.3.4", "1234"}, RXA_AP, 0},
    {{"9.9.9.9", "1234"}, {"1.2.3.4", "1234"}, RXA_ADDR, 0},
    {{"9.9.9.9", "1234"}, {"1.2.3.4", "1234"}, RXA_PORT, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "9999"}, RXA_ADDR, 1},
    {{"1.2.3.4", "1234"}, {"1.2.3.4", "9999"}, RXA_PORT, 0},
    {{"1:2:3:4:5:6:7:8", "1234"}, {"1:2:3:4:5:6:7:8", "1234"}, RXA_ALL, 1},
    {{"1:2:3:4:5:6:7:8", "1234"}, {"0:0:0:0:0:0:1234:abcd", "1234"}, RXA_ALL, 0},
    {{"18.52.171.205", "1234"}, {"0:0:0:0:0:0:1234:abcd", "1234"}, RXA_ALL, 1},
    {{"18.52.171.205", "1234"}, {"0:0:0:0:0:ffff:1234:abcd", "1234"}, RXA_ALL, 1},
    {{NULL}}
};

void
test_sockaddr_compare(void)
{
    struct sockaddr_compare_test_case *tc;
    struct rx_sockaddr a;
    struct rx_sockaddr b;

    for (tc = sockaddr_compare_test_cases; tc->a.addr; tc++) {
	make_sockaddr(tc->a.addr, tc->a.port, &a);
	make_sockaddr(tc->b.addr, tc->b.port, &b);
	ok(rx_compare_sockaddr(&a, &b, tc->mask) == tc->expect,
	   "compare sockaddr: [%s]:%s <=> [%s]:%s is %s with mask 0x%02x",
	   tc->a.addr, tc->a.port, tc->b.addr, tc->b.port,
	   tc->expect ? "equal" : "not equal", tc->mask);
    }
}

struct address_serialize_test_case {
    const char *addr;
    unsigned char bin_addr[16];
} address_serialize_test_cases[] = {
    {"0.0.0.0", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0}},
    {"1.2.3.4", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 2, 3, 4}},
    {"1:2:3:4:5:6:7:8", {0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8}},
    {NULL}
};

void
test_address_serialize(void)
{
    struct address_serialize_test_case *tc;
    struct rx_address a;
    unsigned char got_addr[16];

    for (tc = address_serialize_test_cases; tc->addr; tc++) {
	make_address(tc->addr, &a);
	rx_serialize_address(&a, got_addr, sizeof(got_addr));
	ok(memcmp(got_addr, tc->bin_addr, 16) == 0,
	    "serialize address: %s", tc->addr);
    }
}

void
test_address_deserialize(void)
{
    struct address_serialize_test_case *tc;
    struct rx_address expect;
    struct rx_address got_a;

    for (tc = address_serialize_test_cases; tc->addr; tc++) {
	make_address(tc->addr, &expect);
	rx_deserialize_address(tc->bin_addr, &got_a);
	ok(rx_compare_address(&got_a, &expect) != 0,
	    "deserialize sockaddr: %s", tc->addr);
    }
}

struct address_compare_test_case {
    const char *a;
    const char *b;
    int expect;
} address_compare_test_cases[] = {
    {"0.0.0.0", "0.0.0.0", 1},
    {"1.2.3.4", "1.2.3.4", 1},
    {"9.9.9.9", "1.2.3.4", 0},
    {"1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8", 1},
    {"1:2:3:4:5:6:7:8", "0:0:0:0:0:0:1234:abcd", 0},
    {"18.52.171.205", "0:0:0:0:0:0:1234:abcd", 1},
    {"18.52.171.205", "0:0:0:0:0:ffff:1234:abcd", 1},
    {NULL}
};

void
test_address_compare(void)
{
    struct address_compare_test_case *tc;
    struct rx_address a;
    struct rx_address b;

    for (tc = address_compare_test_cases; tc->a; tc++) {
	make_address(tc->a, &a);
	make_address(tc->b, &b);
	ok(rx_compare_address(&a, &b) == tc->expect,
	   "compare address: %s <=> %s is %s",
	   tc->a, tc->b, tc->expect ? "equal" : "not equal");
    }
}

void
test_sockaddr_to_address(void)
{
    struct rx_sockaddr sa;
    struct rx_address a;
    char buf[64];

    make_sockaddr("1.2.3.4", "1234", &sa);
    rx_sockaddr_to_address(&sa, &a);
    is_string("1.2.3.4", rx_print_address(&a, buf, sizeof(buf)),
	"sockaddr to address v4");

    make_sockaddr("1:2:3:4:5:6:7:8", "1234", &sa);
    rx_sockaddr_to_address(&sa, &a);
    is_string("1:2:3:4:5:6:7:8", rx_print_address(&a, buf, sizeof(buf)),
	"sockaddr to address v6");
}

void
test_address_to_sockaddr(void)
{
    struct rx_address a;
    struct rx_sockaddr sa;
    char buf[64];

    make_address("1.2.3.4", &a);
    rx_address_to_sockaddr(&a, htons(1234), 1, &sa);
    is_string("1.2.3.4:1234", rx_print_sockaddr(&sa, buf, sizeof(buf)),
	"address to sockaddr v4");

    make_address("1:2:3:4:5:6:7:8", &a);
    rx_address_to_sockaddr(&a, htons(1234), 1, &sa);
    is_string("[1:2:3:4:5:6:7:8]:1234", rx_print_sockaddr(&sa, buf, sizeof(buf)),
	"address to sockaddr v6");
}

int
main(void)
{
    plan(76);

    test_sockaddr_print();
    test_sockaddr_port();
    test_sockaddr_hash();
    test_sockaddr_copy();
    test_sockaddr_to_ipv4();
    test_sockaddr_loopback();
    test_sockaddr_compare();

    test_address_to_ipv4();
    test_address_loopback();
    test_address_compare();
    test_address_serialize();
    test_address_deserialize();

    test_sockaddr_to_address();
    test_address_to_sockaddr();

    return 0;
}
