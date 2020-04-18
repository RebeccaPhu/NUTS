#include "stdafx.h"
#include "EncodingClipboard.h"

BYTE *pClipboard = NULL;
extern time_t ClipStamp  = 0;
extern time_t EClipStamp = 0;
extern DWORD  EClipLen   = 0;

void SetEncodedClipboard( BYTE *pData, DWORD Length )
{
	pClipboard = (BYTE *) realloc( pClipboard, Length );

	memcpy( pClipboard, pData, Length );

	EClipLen = Length;

	EClipStamp = time( NULL );
}
