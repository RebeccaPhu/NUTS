#include "StdAfx.h"

#include "HEXDumpTranslator.h"
#include "libfuncs.h"

#include "NUTSMessages.h"

HEXDumpTranslator::HEXDumpTranslator(void)
{
}


HEXDumpTranslator::~HEXDumpTranslator(void)
{
}

void HEXDumpTranslator::PlaceHex( DWORD hexVal, int digits, BYTE *pPlace )
{
	BYTE hexes[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

	for ( int i=0; i<digits; i++ )
	{
		pPlace[ ( digits - 1 ) - i ] = hexes[ hexVal & 0xF ];

		hexVal &= 0xFFFFFFF0;
		hexVal >>= 4;
	}
}

int HEXDumpTranslator::TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
{
	opts->RetranslateOnResize = true;

	BYTE *pHex = nullptr;

	DWORD TxtSize = 0;

	*opts->pTextBuffer = pHex;
	opts->LinePointers.clear();
	
	// Work out how many characters we can fit. We need ( N * ( 2 for the hex, 1 for the space, 1 for the char, ) )
	// + 3 for the gap characters, + 8 for the address and 1 for the space thereof;

	WORD NumHex = 0;
	WORD BytesPerLine = NumHex * 4 + 3 + 8 + 1;

	while ( BytesPerLine < opts->CharacterWidth )
	{
		NumHex++;

		BytesPerLine = NumHex * 4 + 3 + 8 + 1;

		if ( BytesPerLine >= opts->CharacterWidth )
		{
			NumHex--;
		}
	}

	BytesPerLine = NumHex * 4 + 3 + 8 + 1;

	QWORD FPtr = 0;
	QWORD FExt = FileObj.Ext();
	WORD  CHex = 0;
	BYTE  Buf[ 1024 ];
	DWORD LineStart = 0;

	TxtSize = ( (FExt / NumHex ) + 1 ) * BytesPerLine;

	pHex = (BYTE *) malloc( TxtSize );

	while ( FPtr < FExt )
	{
		if ( WaitForSingleObject( opts->hStop, 0 ) == WAIT_OBJECT_0 )
		{
			/* ABORT! */
			*opts->pTextBuffer   = pHex;
			opts->TextBodyLength = TxtSize;

			return 0;
		}

		DWORD xPtr = ( FPtr % 1024 );

		if ( xPtr == 0 )
		{
			FileObj.Read( Buf, 1024 );

			::PostMessage( opts->ProgressWnd, WM_TEXT_PROGRESS, (WPARAM) Percent( 0, 1, FPtr, FExt, true), 0 );
		}

		if ( CHex == 0 )
		{
			memset( &pHex[ LineStart ], ' ', BytesPerLine );

			PlaceHex( FPtr, 8, &pHex[ LineStart ] );
		}

		PlaceHex( Buf[xPtr], 2, &pHex[ LineStart + ( CHex * 3 ) + 0 + 9 ] );

		pHex[ LineStart + ( CHex * 3 ) + 2 + 9 ] = ' ';

		if ( Buf[ xPtr ] == 0x00 ) { Buf[ xPtr ] = '.'; }

		pHex[ LineStart + ( NumHex * 3) + 3 + CHex + 9 ] = Buf[ xPtr ];

		CHex++;
		FPtr++;

		if ( CHex == NumHex )
		{
			pHex[ LineStart + (NumHex * 3 ) + 0 + 9 ] = ' ';
			pHex[ LineStart + (NumHex * 3 ) + 1 + 9 ] = ' ';
			pHex[ LineStart + (NumHex * 3 ) + 2 + 9 ] = ' ';

			LineStart += BytesPerLine;

			opts->LinePointers.push_back( LineStart );

			CHex = 0;
		}
	}

	if ( NumHex == 0 )
	{
		::PostMessage( opts->ProgressWnd, WM_TEXT_PROGRESS, (WPARAM) 100, 0 );
	}

	*opts->pTextBuffer   = pHex;
	opts->TextBodyLength = TxtSize;

	return 0;
}