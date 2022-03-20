#include "stdafx.h"

#include "libfuncs.h"
#include "BitmapCache.h"

#include <winioctl.h>
#include <process.h>
#include <CommCtrl.h>
#include <stdio.h>
#include <stdarg.h>

int	runningFileID	= 1;

WCHAR *UString(char *pAString) {
	static	WCHAR ustring[1024];

	MultiByteToWideChar(GetACP(), NULL, pAString, -1, ustring, 1023);

	return ustring;
}

char *AString(WCHAR *pUString) {
	static	char astring[1024];
	static  BOOL UseDefault = TRUE;

	WideCharToMultiByte( GetACP(), NULL, pUString, -1, astring, 1023, "_", &UseDefault );

	return astring;
}

char *reverseSlashes(char *ins) {
	static char	outs[8192];

	char	*p	= ins;
	char	*q	= outs;

	while (*p) {
		if (*p == '\\')
			*q	= '/';
		else
			*q	= *p;

		q++;
		p++;
	}

	*q	 = 0;

	return outs;
}

int getFileID() {
	return runningFileID++;
}

bool IsRawFS(WCHAR *path) {
	WCHAR	SysName[16];

	BOOL	SysQ	= GetVolumeInformationW(path , NULL, 0, NULL, 0, NULL, SysName, 15);

	if ((_wcsnicmp(SysName, L"RAW", 3) == 0) || (SysQ == FALSE))
		return true;

	return false;
}

int PhysicalDrive(char *path) {
	HANDLE	hDeviceHandle	= NULL;

	if (strlen(path) > 3)
		return -1;

	if (strncmp(path, "A:", 2) == 0)
		return -2;

	if (strncmp(path, "B:", 2) == 0)
		return -3;

	char	DriveSpec[64];

	sprintf_s(DriveSpec, "\\\\.\\%c:", path[0]);

	hDeviceHandle = CreateFileA(DriveSpec, 0, 0, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDeviceHandle != INVALID_HANDLE_VALUE) {  
		STORAGE_DEVICE_NUMBER sdn;  

		DWORD returned;  

		if (DeviceIoControl(
			hDeviceHandle,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL,
			0,
			&sdn,
			sizeof(sdn),
			&returned,
			NULL)
			) {  
				CloseHandle(hDeviceHandle);

				return sdn.DeviceNumber;
		}

		CloseHandle(hDeviceHandle);
	}

	return -1;
}

int Percent( int stage, int stages, int step, int steps, bool allow100 )
{
	if ( stage > stages ) { stage = stages; }
	if ( step > steps ) { step = steps; }

	double stage_block = 100.0 / (double) stages;
	double step_block  = stage_block / (double) steps;

	double dstep  = (double) step;
	double dstage = (double) stage;

	double percent = ( dstage * stage_block ) + ( dstep * step_block );

	percent++;

	if ( percent > 100 )
	{
		percent	= 100;
	}

	if ( ( percent == 100 ) && ( !allow100 ) )
	{
		percent	= 99;
	}

	return (int) percent;
}

bool ShiftPressed()
{
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		return true;

	if (GetAsyncKeyState(VK_RSHIFT) & 0x8000)
		return true;

	return false;
}

bool FilenameCmp( NativeFile *pFirst, NativeFile *pSecond )
{
	if ( ( pFirst->Flags & FF_Extension) != ( pSecond->Flags & FF_Extension ) )
	{
		return false;
	}

	if ( !rstrnicmp( pFirst->Filename, pSecond->Filename, 256 ) )
	{
		return false;
	}

	if ( pFirst->Flags & FF_Extension )
	{
		if ( !rstrnicmp( pFirst->Extension, pSecond->Extension, 3 ) )
		{
			return false;
		}
	}

	/* Seems identical */
	return true;
}

/* Like sprintf, but for retro filenames, and without the compiler warnings MS seems to think are vital */
BYTE *rsprintf( BYTE *target, BYTE *fmt, ... )
{
	va_list list;

	va_start( list, fmt );

#pragma warning( disable : 4996 )
	vsprintf( (char *) target, (char *) fmt, list );
#pragma warning( default : 4996 )

	va_end( list );

	return target;
}

BYTE *rsprintf( BYTE *target, char *fmt, ... )
{
	va_list list;

	va_start( list, fmt );

#pragma warning( disable : 4996 )
	vsprintf( (char *) target, fmt, list );
#pragma warning( default : 4996 )

	va_end( list );

	return target;
}

DWORD dwDiff( DWORD a, DWORD b )
{
	if ( a == b ) { return 0;  }
	if ( a < b ) { return b - a; }
	
	return a - b;
}

BYTE *rstrncpy( BYTE *target, const BYTE *src, WORD limit )
{
	WORD c = 0;

	while ( 1 )
	{
		if ( c == limit )
		{
			*target = 0;

			return target;
		}

		if ( *src == 0 )
		{
			*target = 0;

			return target;
		}

		*target = *src;

		target++;
		src++;
		c++;
	}

	*target = 0;

	return target;
}

BYTE *rstrnecpy( BYTE *target, BYTE *src, WORD limit, BYTE *exclus )
{
	WORD c = 0;

	while ( 1 )
	{
		if ( c == limit )
		{
			*target = 0;

			return target;
		}

		if ( *src == 0 )
		{
			*target = 0;

			return target;
		}

		BYTE *pChk = exclus;

		while ( *pChk != 0 )
		{
			if ( *src == *pChk )
			{
				break;
			}
			else
			{
				pChk++;
			}
		}

		if ( *pChk == 0 )
		{
			*target = *src;

			target++;
		}

		src++;
		c++;
	}

	*target = 0;

	return target;
}

WORD rstrnlen( BYTE *src, WORD limit )
{
	WORD c = 0;

	while ( ( *src != 0 ) && ( c != limit ) ) { src++; c++; }

	return c;
}

bool rstrcmp( BYTE *first, BYTE *second, bool i )
{
	return rstrncmp( first, second, 0xFF, i );
}

bool rstrncmp( BYTE *first, BYTE *second, WORD limit, bool i )
{
	bool result = true;

	BYTE c = 0;

	while ( c != limit )
	{
		BYTE c1 = *first;
		BYTE c2 = *second;

		if ( i ) 
		{
			if ( ( c1 >= 'a' ) && ( c1 <= 'z' ) )
			{
				c1 &= 0xDF;
			}

			if ( ( c2 >= 'a' ) && ( c2 <= 'z' ) )
			{
				c2 &= 0xDF;
			}
		}

		if ( ( c1 == 0 ) && ( c2 == 0 ) )
		{
			result = true;

			break;
		}

		if ( ( c1 == 0 ) || ( c2 == 0 ) )
		{
			result = false;

			break;
		}

		if ( c1 != c2 )
		{
			result = false;

			break;
		}

		first++;
		second++;
		c++;
	}

	return result;
}

bool rstricmp( BYTE *first, BYTE *second )
{
	return rstrcmp( first, second, true );
}

bool rstrnicmp( BYTE *first, BYTE *second, WORD limit )
{
	return rstrncmp( first, second, limit, true );
}

BYTE *rstrncat( BYTE *target, BYTE *src, WORD limit )
{
	WORD fl = rstrnlen( target, limit );

	if ( fl >= limit )
	{
		return nullptr;
	}

	WORD remain = limit - fl;

	rstrncpy( &target[fl], src, remain );

	return target;
}

BYTE *rstrrchr( BYTE *target, BYTE chr, WORD limit )
{
	WORD tl = rstrnlen( target, limit );

	if ( tl < 1 )
	{
		return nullptr;
	}

	BYTE *srch = &target[ tl - 1 ];

	while ( srch >= target )
	{
		if ( *srch == chr )
		{
			return srch;
		}

		srch--;
	}

	return nullptr;
}

BYTE *rstrndup( BYTE *src, WORD limit )
{
	WORD tl = rstrnlen( src, limit );

	BYTE *target = (BYTE *) malloc( max( 16, tl + 1 ) );

	if ( target != nullptr )
	{
		rstrncpy( target, src, limit );
	}

	return target;
}

DWORD BEDWORD( BYTE *p )
{
	return ( p[0] << 24 ) | ( p[1] << 16 ) | ( p[2] << 8 ) | p[3];
}

WORD BEWORD( BYTE *p )
{
	return ( p[0] << 8 ) | p[1];
}

void WBEDWORD( BYTE *p, DWORD v )
{
	p[ 0 ] = ( ( v & 0xFF000000 ) >> 24 );
	p[ 1 ] = ( ( v & 0xFF0000 ) >> 16 );
	p[ 2 ] = ( ( v & 0xFF00 ) >> 8 );
	p[ 3 ] = v & 0xFF;
}

void WBEWORD( BYTE *p, DWORD v )
{
	p[ 0 ] = ( ( v & 0xFF00 ) >> 8 );
	p[ 1 ] = v & 0xFF;
}

DWORD LEDWORD( BYTE *p )
{
	return ( p[3] << 24 ) | ( p[2] << 16 ) | ( p[1] << 8 ) | p[0];
}

WORD LEWORD( BYTE *p )
{
	return ( p[1] << 8 ) | p[0];
}

void WLEDWORD( BYTE *p, DWORD v )
{
	p[ 3 ] = ( ( v & 0xFF000000 ) >> 24 );
	p[ 2 ] = ( ( v & 0xFF0000 ) >> 16 );
	p[ 1 ] = ( ( v & 0xFF00 ) >> 8 );
	p[ 0 ] = v & 0xFF;
}

void WLEWORD( BYTE *p, DWORD v )
{
	p[ 1 ] = ( ( v & 0xFF00 ) >> 8 );
	p[ 0 ] = v & 0xFF;
}

HWND CreateToolTip( HWND hWnd, HWND hContainer, PTSTR pszText, HINSTANCE hInstance )
{
	if ( ( !hWnd ) || ( !pszText ) )
	{
		return FALSE;
	}
    
	// Create the tooltip. g_hInst is the global instance handle.
	HWND hwndTip = CreateWindowEx(
		NULL, TOOLTIPS_CLASS, NULL,
		WS_POPUP |TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hContainer, NULL,
		hInstance, NULL
	);
    
	if ( !hwndTip )
	{
		return (HWND) NULL;
	}                              
                              
	// Associate the tooltip with the tool.
	TOOLINFO toolInfo = { 0 };

	toolInfo.cbSize   = sizeof(toolInfo);
	toolInfo.hwnd     = hContainer;
	toolInfo.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId      = (UINT_PTR) hWnd;
	toolInfo.lpszText = pszText;

	SendMessage( hwndTip, TTM_ADDTOOL, 0, (LPARAM) &toolInfo );
   
	return hwndTip;
}