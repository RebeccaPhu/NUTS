#include "StdAfx.h"
#include "SpectrumBASICTranslator.h"
#include "../NUTS/libfuncs.h"
#include "../NUTS/NUTSMessages.h"
#include "SinclairDefs.h"

SpectrumBASICTranslator::SpectrumBASICTranslator(void)
{
}


SpectrumBASICTranslator::~SpectrumBASICTranslator(void)
{
}

int SpectrumBASICTranslator::TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
{
	static char *tokens[] = {
		"RND", "INKEY$", "PI", "FN", "POINT", "SCREEN$", "ATTR", "AT", "TAB", "VAL$", "CODE",
		"VAL", "LEN", "SIN", "COS", "TAN", "ASN", "ACS", "ATN", "LN", "EXP", "INT", "SQR", "SGN", "ABS", "PEEK", "IN",
		"USR", "STR$", "CHR$", "NOT", "BIN", "OR", "AND", "<=", ">=", "<>", "LINE", "THEN", "TO", "STEP", "DEF FN", "CAT",
		"FORMAT", "MOVE", "ERASE", "OPEN #", "CLOSE #", "MERGE", "VERIFY", "BEEP", "CIRCLE", "INK", "PAPER", "FLASH", "BRIGHT", "INVERSE", "OVER", "OUT",
		"LPRINT", "LLIST", "STOP", "READ", "DATA", "RESTORE", "NEW", "BORDER", "CONTINUE", "DIM", "REM", "FOR", "GO TO", "GO SUB", "INPUT", "LOAD",
		"LIST", "LET", "PAUSE", "NEXT", "POKE", "PRINT", "PLOT", "RUN", "SAVE", "RANDOMIZE", "IF", "CLS", "DRAW", "CLEAR", "RETURN", "COPY"
	};

	BYTE *pOutBuffer = nullptr;
	DWORD lOutSize   = 0;
	DWORD lOutPtr    = 0;

	DWORD FileSize   = (DWORD) FileObj.Ext();
	DWORD FilePtr    = 0;

	int	o	= 0;

	BYTE DigitCount  = 0;
	BYTE DecodeState = 1;
	WORD BASLine     = 0;
	WORD BASLen      = 0;
	bool IsQuoted    = false;
	BYTE InChar      = 0;
	bool IsKeyword   = false;

	BYTE TranslatedLine[ 1024 ];
	BYTE TranslatedPtr = 0;

	FileObj.Seek( 0 );

	char **tokenSet  = tokens;
	BYTE tokenOffset = 0x80;

	while ( FilePtr < FileSize ) {
		if ( WaitForSingleObject( opts->hStop, 0 ) == WAIT_OBJECT_0 )
		{
			/* ABORT! */
			*opts->pTextBuffer   = pOutBuffer;
			opts->TextBodyLength = lOutSize;

			return 0;
		}

		/* Max line length in BBC BASIC is 255, plus at least 6 chars for the line number, and a zero terminator */
		while ( lOutSize < lOutPtr + 1024 )
		{
			lOutSize   += 1024;

			pOutBuffer = (BYTE *) realloc( pOutBuffer, lOutSize );
		}

		FileObj.Read( &InChar, 1 );
		
		switch ( DecodeState ) {	// state
			case 1: // Line number
				BASLine <<= 8;
				BASLine  |= InChar;

				DigitCount++;

				if ( DigitCount == 2 )
				{
					DecodeState = 2;
					DigitCount  = 0;
					BASLen      = 0;
					tokenSet    = tokens;
				}

				break;

			case 2:	// line length
				BASLen <<= 8;
				BASLen  |= InChar;

				DigitCount++;

				if ( DigitCount == 2 )
				{
					DecodeState = 3;
				}

				break;


			case 3:	// line data
				if ( InChar == 0x0E )
				{
					// Numerical constant - skip the five binary bytes, we'll just use the text
					DecodeState = 4;
					DigitCount  = 0;
				}
				else if ( InChar == 0x0d)
				{
					/* Start of new line */
					if ( FilePtr != 0 ) /* File starts with a 0x0D */
					{
						rsprintf( &pOutBuffer[ lOutPtr ], "%*d%s", 5, BASLine, TranslatedLine );

						lOutPtr += rstrnlen( &pOutBuffer[ lOutPtr ], 1024 );
					}

					DecodeState = 1;
					BASLine     = 0;
					DigitCount  = 0;

					if ( FilePtr != 0 )
					{
						opts->LinePointers.push_back( lOutPtr );
					}

					TranslatedPtr = 0;
					IsQuoted      = false;
					IsKeyword     = false;

					::PostMessage( opts->ProgressWnd, WM_TEXT_PROGRESS, (WPARAM) Percent( 0, 1, FilePtr, FileSize, true ), 0 );

				} else if ( ( InChar == 0x8d ) && ( !IsQuoted ) ) {
					/*
					l	= 0;

					l	|= ((pContent[i + 1] ^ 0x54) & 0x30) << 2;
					l	|= ((pContent[i + 1] ^ 0x54) & 0x03) << 12;

					l	|= (pContent[i + 3] & 0x3f) << 16;

					l	|= pContent[i + 2] & 0x3f;

					i	+= 3;

					sprintf_s(&line[p], 8192 - p, "%d", l);

					p	+= strlen(&line[p]) - 1;
					*/
				} else {
					if ( InChar == '\"')
					{
						IsQuoted = !IsQuoted;
					}

					if ( InChar > 0xA5)  {

						if ( !IsKeyword )
						{
							rsprintf( &TranslatedLine[ TranslatedPtr++ ], " " );
						}

						rsprintf( &TranslatedLine[ TranslatedPtr ], "%s ", (BYTE *) tokens[ InChar - 0xA5 ] );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						tokenSet    = tokens;
						tokenOffset = 0x80;

						IsKeyword   = true;
					}
					else
					{
						TranslatedLine[ TranslatedPtr++ ] = InChar;
						TranslatedLine[ TranslatedPtr   ] = 0;
					}
				}				

				break;

			case 4:
				{
					DigitCount++;

					if ( DigitCount == 5 )
					{
						DecodeState = 3;
					}
				}
				break;
		}

		FilePtr++;
	}

	opts->EncodingID     = ENCODING_SINCLAIR;
	*opts->pTextBuffer   = pOutBuffer;
	opts->TextBodyLength = lOutPtr;

	return 0;
}
