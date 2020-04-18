#ifndef BBC_FUNCTIONS
#define BBC_FUNCTIONS

void BBCStringCopy(char *pDest, char *pSrc, int limit);

// Compare two strings in a BBC-ic way, that is case insensitive.
int bbc_strcmp(char *a, char *b);

#endif