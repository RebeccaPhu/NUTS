#pragma once

#include "defs.h"

WCHAR *UString(char *pAString);
char *AString(WCHAR *pUString);
char *reverseSlashes(char *ins);
int getFileID();
int QueueAction(AppAction &Action);
bool IsRawFS(WCHAR *path);
int PhysicalDrive(char *path);
int Percent(int stage, int stages, int step, int steps, bool allow100);
bool ShiftPressed();
bool FilenameCmp( NativeFile *pFirst, NativeFile *pSecond );

BYTE *rsprintf( BYTE *target, BYTE *fmt, ... );
BYTE *rsprintf( BYTE *target, char *fmt, ... );

BYTE *rstrncpy( BYTE *target, BYTE *src, WORD limit );
BYTE *rstrnecpy( BYTE *target, BYTE *src, WORD limit, BYTE *exclus );
BYTE *rstrncat( BYTE *target, BYTE *src, WORD limit );
BYTE *rstrrchr( BYTE *target, BYTE chr, WORD limit );
WORD rstrnlen( BYTE *src, WORD limit );
bool rstrcmp( BYTE *first, BYTE *second, bool i = false );
bool rstrncmp( BYTE *first, BYTE *second, WORD limit, bool i = false );
bool rstricmp( BYTE *first, BYTE *second );
bool rstrnicmp( BYTE *first, BYTE *second, WORD limit );
BYTE *rstrndup( BYTE *src, WORD limit );

#define rstrcpy( target, src ) rstrncpy( target, src, 0xFFFF )

DWORD BEDWORD( BYTE *p );
WORD BEWORD( BYTE *p );

void WBEDWORD( BYTE *p, DWORD v );
void WBEWORD( BYTE *p, DWORD v );

DWORD dwDiff( DWORD a, DWORD b );

HWND CreateToolTip( HWND hWnd, HWND hContainer, PTSTR pszText, HINSTANCE hInstance );

class AutoBuffer
{
public:
	AutoBuffer( DWORD sz )
	{
		intsz = sz;
		intp  = malloc( sz );
	}

	AutoBuffer( const AutoBuffer &source )
	{
		intsz = source.intsz;
		intp  = malloc( intsz );

		(void) memcpy( intp, source.intp, intsz );
	}

	~AutoBuffer()
	{
		free( intp );
	}

	operator BYTE *() { return (BYTE *) intp; }
	operator void *() { return (BYTE *) intp; }

private:
	void *intp;
	DWORD intsz;
};
