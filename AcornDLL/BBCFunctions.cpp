#include "stdafx.h"
#include "BBCFunctions.h"

void BBCStringCopy(char *pDest, char *pSrc, int limit) {
	int n	= 0;

	while ( (n<limit) && (*pSrc) && (*pSrc != 0x0D) ) {
		*pDest++	= *pSrc++;

		n++;
	}

	if (n < limit)
		*pDest	= 13;
}

// Compare two strings in a BBC-ic way, that is case insensitive.
int bbc_strcmp( BYTE *a, BYTE *b ) {
	int i = 0;

	char ca,cb;

	while (1) {
		ca = *a;
		cb = *b;

		if (ca == 0 || cb == 0) {
			return 0;
		}

		if (ca == 0 && cb != 0) {
			return 1;
		}

		if (ca != 0 && cb == 0) {
			return -1;
		}

		if (ca >= 'a' && ca <= 'z') { ca &= 0xDF; }
		if (cb >= 'a' && cb <= 'z') { cb &= 0xDF; }

		if (cb > ca) {
			return -1;
		}

		if (ca > cb) {
			return 1;
		}

		a++;
		b++;
	}

	// Never reached
	return 0;
}
