#ifndef BBC_FUNCTIONS
#define BBC_FUNCTIONS

void BBCStringCopy( BYTE *pDest, BYTE *pSrc, int limit );

// Compare two strings in a BBC-ic way, that is case insensitive.
int bbc_strcmp( BYTE *a, BYTE *b );

#endif