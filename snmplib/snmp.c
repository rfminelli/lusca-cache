/*
 * Simple Network Management Protocol (RFC 1067).
 *
 */
/**********************************************************************
	Copyright 1988, 1989, 1991, 1992 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
#include <stdio.h>

#ifdef KINETICS
#include "gw.h"
#include "ab.h"
#include "inet.h"
#include "fp4/cmdmacro.h"
#include "fp4/pbuf.h"
#include "glob.h"
#endif

#ifdef linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <malloc.h>
#endif

#if (defined(unix) && !defined(KINETICS))
#include <sys/types.h>
#include <netinet/in.h>
#ifndef NULL
#define NULL 0
#endif
#endif

#ifdef vms
#include <in.h>
#endif

#include "asn1.h"
#include "snmp.h"
#include "snmp_impl.h"

#include "mib.h"

void
shift_array(begin, length, shift_amount)
    u_char	    *begin;
    int    length;
    int		    shift_amount;
{
    u_char	*old, *new;

    if (shift_amount >= 0){
	old = begin + length - 1;
	new = old + shift_amount;

	while(length--)
	    *new-- = *old--;
    } else {
	old = begin;
	new = begin + shift_amount;

	while(length--)
	    *new++ = *old++;
    }
}


void
xdump(cp, length, prefix)
    u_char *cp;
    int length;
    char *prefix;
{
    int col, count;

    count = 0;
    while(count < length){
	printf("%s", prefix);
	for(col = 0;count + col < length && col < 16; col++){
	    if (col != 0 && (col % 4) == 0)
		printf(" ");
	    printf("%02X ", cp[count + col]);
	}
	while(col++ < 16){	/* pad end of buffer with zeros */
	    if ((col % 4) == 0)
		printf(" ");
	    printf("   ");
	}
	printf("  ");
	for(col = 0;count + col < length && col < 16; col++){
	    if (isprint(cp[count + col]))
		printf("%c", cp[count + col]);
	    else
		printf(".");
	}
	printf("\n");
	count += col;
    }

}



u_char *
snmp_parse_var_op(data, var_name, var_name_len, var_val_type, var_val_len, var_val, listlength)
    u_char *data;	    /* IN - pointer to the start of object */
    oid	    *var_name;	    /* OUT - object id of variable */
    int	    *var_name_len;  /* IN/OUT - length of variable name */
    u_char  *var_val_type;  /* OUT - type of variable (int or octet string) (one byte) */
    int	    *var_val_len;   /* OUT - length of variable */
    u_char  **var_val;	    /* OUT - pointer to ASN1 encoded value of variable */
    int	    *listlength;    /* IN/OUT - number of valid bytes left in var_op_list */
{
    u_char	    var_op_type;
    int		    var_op_len = *listlength;
    u_char	    *var_op_start = data;

    data = asn_parse_header(data, &var_op_len, &var_op_type);
    if (data == NULL){
	ERROR("snmp_parse_var_op(): 1 asn_parse_header() == NULL");
	return NULL;
    }
    if (var_op_type != (u_char)(ASN_SEQUENCE | ASN_CONSTRUCTOR))
	return NULL;
    data = asn_parse_objid(data, &var_op_len, &var_op_type, var_name, var_name_len);
    if (data == NULL){
	ERROR("snmp_parse_var_op(): asn_parse_objid() == NULL");
	return NULL;
    }
    if (var_op_type != (u_char)(ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OBJECT_ID))
	return NULL;
    *var_val = data;	/* save pointer to this object */
    /* find out what type of object this is */
    data = asn_parse_header(data, &var_op_len, var_val_type);
    if (data == NULL){
	ERROR("snmp_parse_var_op(): 2 asn_parse_header() == NULL");
	return NULL;
    }
    *var_val_len = var_op_len;
    data += var_op_len;
    *listlength -= (int)(data - var_op_start);
    return data;
}

u_char *
snmp_build_var_op(data, var_name, var_name_len, var_val_type, var_val_len,
		  var_val, listlength)
    u_char	*data;		/* IN - pointer to the beginning of the output buffer */
    oid		*var_name;	/* IN - object id of variable */
    int		*var_name_len;	/* IN - length of object id */
    u_char	var_val_type;	/* IN - type of variable */
    int		var_val_len;	/* IN - length of variable */
    u_char	*var_val;	/* IN - value of variable */
    int		*listlength;	/* IN/OUT - number of valid bytes left in
				   output buffer */
{
    int		    dummyLen, headerLen;
    u_char	    *dataPtr;

    dummyLen = *listlength;
    dataPtr = data;
#if 0
    data = asn_build_sequence(data, &dummyLen,
			      (u_char)(ASN_SEQUENCE | ASN_CONSTRUCTOR), 0);
    if (data == NULL){
	ERROR("");
	return NULL;
    }
#endif
    data += 4;
    dummyLen -=4;
    if (dummyLen < 0)
	return NULL;

    headerLen = data - dataPtr;
    *listlength -= headerLen;
    data = asn_build_objid(data, listlength,
	    (u_char)(ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OBJECT_ID),
	    var_name, *var_name_len);
    if (data == NULL){
	ERROR("");
	return NULL;
    }
    switch(var_val_type){
	case ASN_INTEGER:
	    data = asn_build_int(data, listlength, var_val_type,
		    (long *)var_val, var_val_len);
	    break;
	case GAUGE:
	case COUNTER:
	case TIMETICKS:
	case UINTEGER:
	    data = asn_build_unsigned_int(data, listlength, var_val_type,
					  (u_long *)var_val, var_val_len);
	    break;
	case COUNTER64:
	    data = asn_build_unsigned_int64(data, listlength, var_val_type,
					   (struct counter64 *)var_val,
					    var_val_len);
	    break;
	case ASN_OCTET_STR:
	case IPADDRESS:
	case OPAQUE:
        case NSAP:
	    data = asn_build_string(data, listlength, var_val_type,
		    var_val, var_val_len);
	    break;
	case ASN_OBJECT_ID:
	    data = asn_build_objid(data, listlength, var_val_type,
		    (oid *)var_val, var_val_len / sizeof(oid));
	    break;
	case ASN_NULL:
	    data = asn_build_null(data, listlength, var_val_type);
	    break;
	case ASN_BIT_STR:
	    data = asn_build_bitstring(data, listlength, var_val_type,
		    var_val, var_val_len);
	    break;
	case SNMP_NOSUCHOBJECT:
	case SNMP_NOSUCHINSTANCE:
	case SNMP_ENDOFMIBVIEW:
	    data = asn_build_null(data, listlength, var_val_type);
	    break;
	default:
	    ERROR("wrong type");
	    return NULL;
    }
    if (data == NULL){
	ERROR("");
	return NULL;
    }
    dummyLen = (data - dataPtr) - headerLen;

    asn_build_sequence(dataPtr, &dummyLen,
		       (u_char)(ASN_SEQUENCE | ASN_CONSTRUCTOR), dummyLen);
    return data;
}


