#pragma once

#include "defs.h"

void BBCStringCopy(char *pDest, char *pSrc, int limit);
WCHAR *UString(char *pAString);
char *AString(WCHAR *pUString);
char *reverseSlashes(char *ins);
int getFileID();
int QueueAction(AppAction &Action);
bool IsRawFS(WCHAR *path);
int PhysicalDrive(char *path);
int Percent(int stage, int stages, int step, int steps, bool allow100);
bool ShiftPressed();
int bbc_strcmp(char *a, char *b);
bool FilenameCmp( NativeFile *pFirst, NativeFile *pSecond );

BYTE *rsprintf( BYTE *target, BYTE *fmt, ... );
BYTE *rsprintf( BYTE *target, char *fmt, ... );

BYTE *rstrncpy( BYTE *target, BYTE *src, WORD limit );
BYTE *rstrcpy( BYTE *target, BYTE *src );
BYTE *rstrncat( BYTE *target, BYTE *src, WORD limit );
BYTE *rstrrchr( BYTE *target, BYTE chr, WORD limit );
WORD rstrnlen( BYTE *src, WORD limit );
bool rstrcmp( BYTE *first, BYTE *second, bool i = false );
bool rstrncmp( BYTE *first, BYTE *second, BYTE limit, bool i = false );
bool rstricmp( BYTE *first, BYTE *second );
bool rstrnicmp( BYTE *first, BYTE *second, BYTE limit );
BYTE *rstrndup( BYTE *src, WORD limit );

DWORD dwDiff( DWORD a, DWORD b );

