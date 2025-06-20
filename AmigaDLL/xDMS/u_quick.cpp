#include "StdAfx.h"

/*
 *     xDMS  v1.3  -  Portable DMS archive unpacker  -  Public Domain
 *     Written by     Andre Rodrigues de la Rocha  <adlroc@usa.net>
 *
 *
 */


#include <string.h>

#include "cdata.h"
#include "u_quick.h"
#include "getbits.h"


#define QBITMASK 0xff


USHORT quick_text_loc;


USHORT Unpack_QUICK(UCHAR *in, UCHAR *out, USHORT origsize){
	USHORT i, j;
	UCHAR *outend;

	initbitbuf(in);

	outend = out+origsize;
	while (out < outend) {
		if (GETBITS(1)!=0) {
			DROPBITS(1);
			*out++ = text[quick_text_loc++ & QBITMASK] = (UCHAR)GETBITS(8);  DROPBITS(8);
		} else {
			DROPBITS(1);
			j = (USHORT) (GETBITS(2)+2);  DROPBITS(2);
			i = (USHORT) (quick_text_loc - GETBITS(8) - 1);  DROPBITS(8);
			while(j--) {
				*out++ = text[quick_text_loc++ & QBITMASK] = text[i++ & QBITMASK];
			}
		}
	}
	quick_text_loc = (USHORT)((quick_text_loc+5) & QBITMASK);

	return 0;
}

