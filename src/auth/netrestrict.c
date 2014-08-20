/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Network utility functions
 * Parsing NetRestrict file and filtering IP addresses
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <ctype.h>

#include <afs/opr.h>

#include <rx/rx.h>
#include <afs/dirpath.h>

#include "cellconfig.h"

#define AFS_IPINVALID        0xffffffff	/* invalid IP address */
#define AFS_IPINVALIDIGNORE  0xfffffffe	/* no input given to extractAddr */
#define MAX_NETFILE_LINE       2048	/* length of a line in the netrestrict file */
#define MAXIPADDRS             1024	/* from afsd.c */

static int ParseNetInfoFile_int(struct rx_sockaddr *, struct rx_sockaddr *, afs_uint32 *,
                         int, char reason[], const char *,
                         int);
/*
 * The line parameter is a pointer to a buffer containing a string of
 * bytes of the form
** w.x.y.z 	# machineName
 * returns the network interface IP Address in NBO
 */
afs_uint32
extract_Addr(char *line, int maxSize)
{
    char bytes[4][32];
    int i = 0, n = 0;
    char *endPtr;
    afs_uint32 val[4];
    afs_uint32 retval = 0;

    /* skip empty spaces */
    while (isspace(*line) && maxSize) {
	line++;
	maxSize--;
    }
    /* skip empty lines */
    if (!maxSize || !*line)
	return AFS_IPINVALIDIGNORE;

    for (n = 0; n < 4; n++) {
	while ((*line != '.') && !isspace(*line) && maxSize) {	/* extract nth byte */
	    if (!isdigit(*line))
		return AFS_IPINVALID;
	    if (i > 31)
		return AFS_IPINVALID;	/* no space */
	    bytes[n][i++] = *line++;
	    maxSize--;
	}			/* while */
	if (!maxSize)
	    return AFS_IPINVALID;
	bytes[n][i] = 0;
	i = 0, line++;
	errno = 0;
	val[n] = strtol(bytes[n], &endPtr, 10);
	if ((val[n] == 0) && (errno != 0 || bytes[n] == endPtr))	/* no conversion */
	    return AFS_IPINVALID;
    }				/* for */

    retval = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
    return htonl(retval);
}




/* parseNetRestrictFile()
 * Get a list of IP addresses for this host removing any address found
 * in the config file (fileName parameter): /usr/vice/etc/NetRestrict
 * for clients and /usr/afs/local/NetRestrict for servers.
 *
 * Returns the number of valid addresses in outAddrs[] and count in
 * nAddrs.  Returns 0 on success; or 1 if the config file was not
 * there or empty (we still return the host's IP addresses). Returns
 * -1 on fatal failure with reason in the reason argument (so the
 * caller can choose to ignore the entire file but should write
 * something to a log file).
 *
 * All addresses should be in NBO (as returned by rx_getAllAddrMaskMtu() and
 * parsed by extract_Addr().
 */
/*
  afs_uint32  outAddrs[];          * output address array *
  afs_uint32  *mask, *mtu;         * optional mask and mtu *
  afs_uint32 maxAddrs;	   	   * max number of addresses *
  afs_uint32 *nAddrs;              * number of Addresses in output array *
  char       reason[];             * reason for failure *
  const char *fileName;            * filename to parse *
*/

static int
parseNetRestrictFile_int(struct rx_sockaddr outAddrs[], struct rx_sockaddr * mask,
			 afs_uint32 * mtu, afs_uint32 maxAddrs,
			 afs_uint32 * nAddrs, char reason[],
			 const char *fileName, const char *fileName_ni)
{
    FILE *fp;
    char line[MAX_NETFILE_LINE];
    int lineNo, usedfile = 0;
    afs_uint32 i, neaddrs, nOutaddrs;
    rx_in_addr_t addr;
    afs_uint32 eMtu[MAXIPADDRS];
    struct rx_sockaddr eAddrs[MAXIPADDRS], eMask[MAXIPADDRS];

    opr_Assert(outAddrs);
    opr_Assert(reason);
    opr_Assert(fileName);
    opr_Assert(nAddrs);
    if (mask)
	opr_Assert(mtu);

    /* Initialize */
    *nAddrs = 0;
    for (i = 0; i < maxAddrs; i++)
	memset(&outAddrs[i], 0, sizeof(struct rx_sockaddr));
    strcpy(reason, "");

    /* get all network interfaces from the kernel */
    neaddrs = rx_getAllAddrMaskMtu2(eAddrs, eMask, eMtu, MAXIPADDRS);
    if (neaddrs <= 0) {
	sprintf(reason, "No existing IP interfaces found");
	return -1;
    }
    i = 0;
    if ((neaddrs < MAXIPADDRS) && fileName_ni)
	i = ParseNetInfoFile_int(&(eAddrs[neaddrs]), &(eMask[neaddrs]),
				 &(eMtu[neaddrs]), MAXIPADDRS-neaddrs, reason,
				 fileName_ni, 1);

    if (i > 0)
	neaddrs += i;

    if ((fp = fopen(fileName, "r")) == 0) {
	sprintf(reason, "Could not open file %s for reading:%s", fileName,
		strerror(errno));
	goto done;
    }

    /* For each line in the NetRestrict file */
    lineNo = 0;
    usedfile = 0;
    while (fgets(line, MAX_NETFILE_LINE, fp) != NULL) {
	lineNo++;		/* input line number */
	addr = extract_Addr(line, strlen(line));
	if (addr == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "%s : line %d : parse error - invalid IP\n",
		    fileName, lineNo);
	    continue;
	}
	if (addr == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    fprintf(stderr, "%s : line %d : invalid address ... ignoring\n",
		    fileName, lineNo);
	    continue;
	}
	usedfile = 1;

	/* Check if we need to exclude this address */
	for (i = 0; i < neaddrs; i++) {
	    if (eAddrs[i].addr.sin.sin_addr.s_addr && (eAddrs[i].addr.sin.sin_addr.s_addr == addr)) {
		rx_ipv4_to_sockaddr(0, 0, 0, &eAddrs[i]); /* Yes - exclude it by zeroing it for now */
	    }
	}
    }				/* while */

    fclose(fp);

    if (!usedfile) {
	sprintf(reason, "No valid IP addresses in %s\n", fileName);
	goto done;
    }

  done:
    /* Collect the addresses we have left to return */
    nOutaddrs = 0;
    for (i = 0; i < neaddrs; i++) {
	if (!eAddrs[i].addr.sin.sin_addr.s_addr)
	    continue;
	rx_copy_sockaddr(&eAddrs[i], &outAddrs[nOutaddrs]);
	if (mask) {
	    rx_copy_sockaddr(&eMask[i], &mask[nOutaddrs]);
	    mtu[nOutaddrs] = eMtu[i];
	}
	if (++nOutaddrs >= maxAddrs)
	    break;
    }
    if (nOutaddrs == 0) {
	sprintf(reason, "No addresses to use after parsing %s", fileName);
	return -1;
    }
    *nAddrs = nOutaddrs;
    return (usedfile ? 0 : 1);	/* 0=>used the file.  1=>didn't use file */
}

int
afsconf_ParseNetRestrictFile(struct rx_sockaddr outAddrs[], struct rx_sockaddr * mask,
			     afs_uint32 * mtu, afs_uint32 maxAddrs,
			     afs_uint32 * nAddrs, char reason[],
			     const char *fileName)
{
    return parseNetRestrictFile_int(outAddrs, mask, mtu, maxAddrs, nAddrs, reason, fileName, NULL);
}

/*
 * this function reads in stuff from InterfaceAddr file in
 * /usr/vice/etc ( if it exists ) and verifies the addresses
 * specified.
 * 'final' contains all those addresses that are found to
 * be valid. This function returns the number of valid
 * interface addresses. Pulled out from afsd.c
 */
static int
ParseNetInfoFile_int(struct rx_sockaddr * final, struct rx_sockaddr * mask, afs_uint32 * mtu,
		     int max, char reason[], const char *fileName,
		     int fakeonly)
{

    struct rx_sockaddr existingAddr[MAXIPADDRS], existingMask[MAXIPADDRS];
    afs_uint32 existingMtu[MAXIPADDRS];
    char line[MAX_NETFILE_LINE];
    FILE *fp;
    int i, existNu, count = 0;
    afs_uint32 addr;
    int lineNo = 0;
    int l;

    opr_Assert(fileName);
    opr_Assert(final);
    opr_Assert(mask);
    opr_Assert(mtu);
    opr_Assert(reason);

    /* get all network interfaces from the kernel */
    existNu =
	rx_getAllAddrMaskMtu2(existingAddr, existingMask, existingMtu,
			      MAXIPADDRS);
    if (existNu < 0)
	return existNu;

    if ((fp = fopen(fileName, "r")) == 0) {
	/* If file does not exist or is not readable, then
	 * use all interface addresses.
	 */
	sprintf(reason,
		"Failed to open %s(%s)\nUsing all configured addresses\n",
		fileName, strerror(errno));
	for (i = 0; i < existNu; i++) {
	    rx_copy_sockaddr(&existingAddr[i], &final[i]);
	    rx_copy_sockaddr(&existingMask[i], &mask[i]);
	    mtu[i] = existingMtu[i];
	}
	return existNu;
    }

    /* For each line in the NetInfo file */
    while (fgets(line, MAX_NETFILE_LINE, fp) != NULL) {
	int fake = 0;

	/* See if first char is an 'F' for fake */
	/* Added to allow the fileserver to advertise fake IPS for use with
	 * the translation tables for NAT-like firewalls - defect 12462 */
	for (fake = 0; ((fake < strlen(line)) && isspace(line[fake]));
	     fake++);
	if ((fake < strlen(line))
	    && ((line[fake] == 'f') || (line[fake] == 'F'))) {
	    fake++;
	} else {
	    fake = 0;
	}

	lineNo++;		/* input line number */
	addr = extract_Addr(&line[fake], strlen(&line[fake]));

	if (addr == AFS_IPINVALID) {	/* syntactically invalid */
	    fprintf(stderr, "afs:%s : line %d : parse error\n", fileName,
		    lineNo);
	    continue;
	}
	if (addr == AFS_IPINVALIDIGNORE) {	/* ignore error */
	    continue;
	}

	/* See if it is an address that really exists */
	for (i = 0; i < existNu; i++) {
	    if (existingAddr[i].addr.sin.sin_addr.s_addr == addr)
		break;
	}
	if ((i >= existNu) && (!fake))
	    continue;		/* not found/fake - ignore */

	/* Check if it is a duplicate address we alread have */
	for (l = 0; l < count; l++) {
	    if (final[l].addr.sin.sin_addr.s_addr == addr) /* this function just works for ipv4 anyway */
		break;
	}
	if (l < count) {
	    fprintf(stderr, "afs:%x specified twice in NetInfo file\n",
		    ntohl(addr));
	    continue;		/* duplicate addr - ignore */
	}

	if (count > max) {	/* no more space */
	    fprintf(stderr,
		    "afs:Too many interfaces. The current kernel configuration supports a maximum of %d interfaces\n",
		    max);
	} else if (fake) {
	    if (!fake)
		fprintf(stderr, "Client (2) also has address %s\n", line);
	    rx_ipv4_to_sockaddr(addr, 0, 0, &final[count]);
	    rx_ipv4_to_sockaddr(0xffffffff, 0, 0, &mask[count]);
	    mtu[count] = htonl(1500);
	    count++;
	} else if (!fakeonly) {
	    rx_copy_sockaddr(&existingAddr[i], &final[count]);
	    rx_copy_sockaddr(&existingMask[i], &mask[count]);
	    mtu[count] = existingMtu[i];
	    count++;
	}
    }				/* while */

    /* in case of any error, we use all the interfaces present */
    if (count <= 0) {
	sprintf(reason,
		"Error in reading/parsing Interface file\nUsing all configured interface addresses \n");
	for (i = 0; i < existNu; i++) {
	    rx_copy_sockaddr(&existingAddr[i], &final[i]);
	    rx_copy_sockaddr(&existingMask[i], &mask[i]);
	    mtu[i] = existingMtu[i];
	}
	return existNu;
    }
    return count;
}

int
afsconf_ParseNetInfoFile(struct rx_sockaddr * final, struct rx_sockaddr * mask, afs_uint32 * mtu,
			 int max, char reason[], const char *fileName)
{
    return ParseNetInfoFile_int(final, mask, mtu, max, reason, fileName, 0);
}

/*
 * Given two arrays of addresses, masks and mtus find the common ones
 * and return them in the first buffer. Return number of common
 * entries.
 */
static int
filterAddrs(struct rx_sockaddr addr1[], struct rx_sockaddr addr2[], struct rx_sockaddr mask1[],
	    struct rx_sockaddr mask2[], afs_uint32 mtu1[], afs_uint32 mtu2[], int n1,
	    int n2)
{
    struct rx_sockaddr taddr[MAXIPADDRS];
    struct rx_sockaddr tmask[MAXIPADDRS];
    afs_uint32 tmtu[MAXIPADDRS];
    int count = 0, i = 0, j = 0, found = 0;

    opr_Assert(addr1);
    opr_Assert(addr2);
    opr_Assert(mask1);
    opr_Assert(mask2);
    opr_Assert(mtu1);
    opr_Assert(mtu2);

    for (i = 0; i < n1; i++) {
	found = 0;
	for (j = 0; j < n2; j++) {	    
	    if (rx_compare_sockaddr(&addr1[i], &addr2[j], RXA_ADDR)) {
		found = 1;
		break;
	    }
	}

	/* Always mask loopback address */
	if (found && rx_is_loopback_sockaddr(&addr1[i]))
	    found = 0;

	if (found) {
	    rx_copy_sockaddr(&addr1[i], &taddr[count]);
	    rx_copy_sockaddr(&mask1[i], &tmask[count]);
	    tmtu[count] = mtu1[i];
	    count++;
	}
    }
    /* copy everything into addr1, mask1 and mtu1 */
    for (i = 0; i < count; i++) {
	rx_copy_sockaddr(&taddr[i], &addr1[i]);
	if (mask1) {
	    rx_copy_sockaddr(&tmask[i], &mask1[i]);
	    mtu1[i] = tmtu[i];
	}
    }
    /* and zero out the rest */
    for (i = count; i < n1; i++) {
	rx_ipv4_to_sockaddr(0, 0, 0, &addr1[i]);
	if (mask1) {
	    rx_ipv4_to_sockaddr(0, 0, 0, &mask1[i]);
	    mtu1[i] = 0;
	}
    }
    return count;
}

/*
 * parse both netinfo and netrerstrict files and return the final
 * set of IP addresses to use
 */
/* max - Entries in addrbuf, maskbuf and mtubuf */

int
afsconf_ParseNetFiles(afs_uint32 addrbuf[], afs_uint32 maskbuf[],
	             afs_uint32 mtubuf[], afs_uint32 max,
		     char reason[], const char *niFileName,
		     const char *nrFileName)
{
    struct rx_sockaddr *addrbuf_ipv4, *maskbuf_ipv4;
    int i, code;

    addrbuf_ipv4 = (struct rx_sockaddr *)calloc(max, sizeof(struct rx_sockaddr));
    maskbuf_ipv4 = (struct rx_sockaddr *)calloc(max, sizeof(struct rx_sockaddr));

    for(i = 0; i < max; i++) {
    	rx_ipv4_to_sockaddr(addrbuf[i], 0, 0, &addrbuf_ipv4[i]);
    	rx_ipv4_to_sockaddr(maskbuf[i], 0, 0, &maskbuf_ipv4[i]);
    }

    code = afsconf_ParseNetFiles2(addrbuf_ipv4, maskbuf_ipv4, mtubuf, max, reason, niFileName, nrFileName);

    free(addrbuf_ipv4);
    free(maskbuf_ipv4);

    return code;
}

int
afsconf_ParseNetFiles2(struct rx_sockaddr addrbuf[], struct rx_sockaddr maskbuf[],
		      afs_uint32 mtubuf[], afs_uint32 max, char reason[],
		      const char *niFileName, const char *nrFileName)
{
    struct rx_sockaddr addrbuf1[MAXIPADDRS], maskbuf1[MAXIPADDRS];
    afs_uint32 mtubuf1[MAXIPADDRS];
    struct rx_sockaddr addrbuf2[MAXIPADDRS], maskbuf2[MAXIPADDRS];
    afs_uint32 mtubuf2[MAXIPADDRS];
    int nAddrs1 = 0;
    afs_uint32 nAddrs2 = 0;
    int code, i;

    nAddrs1 =
	afsconf_ParseNetInfoFile(addrbuf1, maskbuf1, mtubuf1, MAXIPADDRS,
				 reason, niFileName);
    code =
	parseNetRestrictFile_int(addrbuf2, maskbuf2, mtubuf2, MAXIPADDRS,
			     &nAddrs2, reason, nrFileName, niFileName);
    if ((nAddrs1 < 0) && (code)) {
	/* both failed */
	return -1;
    } else if ((nAddrs1 > 0) && (code)) {
	/* netinfo succeeded and netrestrict failed */
	for (i = 0; ((i < nAddrs1) && (i < max)); i++) {
	    rx_copy_sockaddr(&addrbuf1[i], &addrbuf[i]);
	    if (maskbuf) {
		rx_copy_sockaddr(&maskbuf1[i], &maskbuf[i]);
		mtubuf[i] = mtubuf1[i];
	    }
	}
	return i;
    } else if ((!code) && (nAddrs1 < 0)) {
	/* netrestrict succeeded and netinfo failed */
	for (i = 0; ((i < nAddrs2) && (i < max)); i++) {
	    rx_copy_sockaddr(&addrbuf2[i], &addrbuf[i]);
	    if (maskbuf) {
		rx_copy_sockaddr(&maskbuf2[i], &maskbuf[i]);
		mtubuf[i] = mtubuf2[i];
	    }
	}
	return i;
    } else if ((!code) && (nAddrs1 >= 0)) {
	/* both succeeded */
	/* take the intersection of addrbuf1 and addrbuf2 */
	code =
	    filterAddrs(addrbuf1, addrbuf2, maskbuf1, maskbuf2, mtubuf1,
			mtubuf2, nAddrs1, nAddrs2);
	for (i = 0; ((i < code) && (i < max)); i++) {
	    rx_copy_sockaddr(&addrbuf1[i], &addrbuf[i]);
	    if (maskbuf) {
		rx_copy_sockaddr(&maskbuf1[i], &maskbuf[i]);
		mtubuf[i] = mtubuf1[i];
	    }
	}
	return i;
    }
    return 0;
}
