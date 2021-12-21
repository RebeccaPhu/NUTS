#include "stdafx.h"

#include "ISOFuncs.h"
#include "libfuncs.h"

void ISOStrTerm( BYTE *p, WORD l )
{
	p[ l ] = 0;

	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		if ( p[ i ] != 0x20 )
		{
			return;
		}
		else
		{
			p[ i ] = 0x0;
		}
	}
}

void JolietStrTerm( BYTE *jp, WORD jl )
{
	WORD *p = (WORD *) jp;

	WORD l  = jl >> 1;

	p[ l ] = 0;

	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		if ( p[ i ] != 0x0020 )
		{
			break;
		}
		else
		{
			p[ i ] = 0x0;
		}
	}

	// Need to swap the bytes now
	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		WORD v = p[ i ];

		v = ( ( v & 0xFF00 ) >> 8 ) | ( ( v & 0xFF ) << 8 );

		p[ i ] = v;
	}

	BYTE *pUTF = (BYTE *) AString( (WCHAR *) jp );

	rstrncpy( jp, pUTF, l );
}