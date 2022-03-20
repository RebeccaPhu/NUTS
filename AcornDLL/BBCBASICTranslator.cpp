#include "StdAfx.h"
#include "BBCBASICTranslator.h"
#include "../NUTS/libfuncs.h"
#include "Defs.h"
#include "AcornDLL.h"
#include "../NUTS/NUTSMessages.h"

BBCBASICTranslator::BBCBASICTranslator(void)
{
}


BBCBASICTranslator::~BBCBASICTranslator(void)
{
}

int BBCBASICTranslator::TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
{
	static char *tokens[] = {
		"AND", "DIV", "EOR", "MOD", "OR", "ERROR", "LINE", "OFF", "STEP", "SPC", "TAB(", "ELSE", "THEN", "",
		"OPENIN", "PTR", "PAGE", "TIME", "LOMEM", "HIMEM", "ABS", "ACS", "ADVAL", "ASC", "ASN", "ATN", "BGET", "COS", "COUNT",
		"DEG", "ERL", "ERR", "EVAL", "EXP", "EXT", "FALSE", "FN", "GET", "INKEY", "INSTR(", "INT", "LEN", "LN", "LOG",
		"NOT", "OPENUP", "OPENOUT", "PI", "POINT(", "POS", "RAD", "RND", "SGN", "SIN", "SQR", "TAN", "TO", "TRUE", "USR",
		"VAL", "VPOS", "CHR$", "GET$", "INKEY$", "LEFT$(", "MID$(", "RIGHT$(", "STR$", "STRING$(", "", "", "",
		"LIST", "NEW", "OLD", "RENUMBER", "SAVE", "EDIT", "PUT", "PTR", "PAGE", "TIME", "LOMEM", "HIMEM", "SOUND",
		"BPUT", "CALL", "CHAIN", "CLEAR", "CLOSE", "CLG", "CLS", "DATA", "DEF", "DIM", "DRAW", "END", "ENDPROC", "ENVELOPE",
		"FOR", "GOSUB", "GOTO", "GCOL", "IF", "INPUT", "LET", "LOCAL", "MODE", "MOVE", "NEXT", "ON", "VDU", "PLOT", "PRINT",
		"PROC", "READ", "REM", "REPEAT", "REPORT", "RESTORE", "RETURN", "RUN", "STOP", "COLOUR", "TRACE", "UNTIL", "WIDTH", "OSCLI"
	};

	static char *esccfn[] = {
		"SUM", "BEAT"
	};

	static char *esccom[] = {
		"APPEND", "AUTO", "CRUNCH", "DELET", "EDIT", "HELP", "LIST", "LOAD", "LVAR", "NEW", "OLD",
		"RENUMBER", "SAVE", "TEXTLOAD", "TEXTSAVE", "TWIN", "TWINO", "INSTALL"
	};

	static char *escstmt[] = {
		"CASE", "CIRCLE", "FILL", "ORIGIN", "PSET", "RECT", "SWAP", "WHILE", "WAIT", "MOUSE", "QUIT",
		"SYS", "INSTALL", "LIBRARY", "TINT", "ELLIPSE", "BEATS", "TEMPO", "VOICES", "VOICE", "STEREO", "OVERLAY"
	};

	BYTE *pOutBuffer = nullptr;
	DWORD lOutSize   = 0;
	DWORD lOutPtr    = 0;

	DWORD FileSize   = (DWORD) FileObj.Ext();
	DWORD FilePtr    = 0;

	int	o	= 0;

	BYTE DigitCount  = 0;
	BYTE DecodeState = 3; // Waiting for new line
	WORD BASLine     = 0;
	WORD BASLen      = 0;
	bool IsQuoted    = false;
	BYTE InChar      = 0;

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

				if ( DigitCount == 1 )
				{
					DecodeState = 3;
				}

				break;


			case 3:	// line data
				if ( InChar == 0x0d)
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

					if ( ( InChar > 0x7f) && ( !IsQuoted ) ) {
						if ( InChar == 0xC6 )
						{
							tokenSet    = esccfn;
							tokenOffset = 0x8E;

							FilePtr++;
							
							continue;
						}
						else if ( InChar == 0xC7 )
						{
							tokenSet    = esccom;
							tokenOffset = 0x8E;

							FilePtr++;
							
							continue;
						}
						else if ( InChar == 0xC8 )
						{
							tokenSet    = escstmt;
							tokenOffset = 0x8E;

							FilePtr++;
							
							continue;
						}

						rsprintf( &TranslatedLine[ TranslatedPtr ], (BYTE *) tokenSet[ InChar - tokenOffset ] );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						tokenSet    = tokens;
						tokenOffset = 0x80;
					}
					else
					{
						TranslatedLine[ TranslatedPtr++ ] = InChar;
						TranslatedLine[ TranslatedPtr   ] = 0;
					}
				}				

				break;
		}

		FilePtr++;
	}

	opts->EncodingID     = ENCODING_ACORN;
	*opts->pTextBuffer   = pOutBuffer;
	opts->TextBodyLength = lOutPtr;

	return 0;
}
