#include "StdAfx.h"
#include "LocomotiveBASICTranslator.h"
#include "../NUTS/libfuncs.h"
#include "Defs.h"

#include <math.h>

LocomotiveBASICTranslator::LocomotiveBASICTranslator(void)
{
}


LocomotiveBASICTranslator::~LocomotiveBASICTranslator(void)
{
}

int LocomotiveBASICTranslator::TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
{
	static char *lowtokens[] = {
		"", ":", "%", "$", "!", "", "", "", "", "", "", "", "", "", "0",
		"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "", "", "", "&X", "&H", "", "n", "f"
	};

	static char *tokens[] = {
		"AFTER", "AUTO", "BORDER", "CALL", "CAT", "CHAIN", "CLEAR", "CLG",
		"CLOSEIN", "CLOSEOUT", "CLS", "CONT", "DATA", "DEF", "DEFINT", "DEFREAL",
		"DEFSTR", "DEG", "DELETE", "DIM", "DRAW", "DRAWR", "EDIT", "ELSE", 
		"END", "ENT", "ENV", "ERASE", "ERROR", "EVERY", "FOR", "GOSUB",
		"GOTO", "IF", "INK", "INPUT", "KEY", "LET", "LINE", "LIST",
		"LOAD", "LOCATE", "MEMORY", "MERGE", "MID$", "MODE", "MOVE", "MOVER",
		"NEXT", "NEW", "ON", "ON BREAK", "ON ERROR GOTO", "ON SQ", "OPENIN", "OPENOUT",
		"ORIGIN", "OUT", "PAPER", "PEN", "PLOT", "PLOTR", "POKE", "PRINT",
		"\"\"", "RAD", "RANDOMIZE", "READ", "RELEASE", "REM", "RENUM", "RESTORE",
		"RESUME", "RETURN", "RUN", "SAVE", "SOUND", "SPEED", "STOP", "SYMBOL",
		"TAG", "TAGOFF", "TROFF", "TRON", "WAIT", "WEND", "WHILE", "WIDTH",
		"WINDOW", "WRITE", "ZONE", "DI", "EI", "FILL", "GRAPHICS", "MASK",
		"FRAME", "CURSOR", "x", "ERL", "FN", "SPC", "STEP", "SWAP",
		"x", "x", "TAB", "THEN", "TO", "USING", ">", "=",
		">=", "<", "<>", "<=", "+", "-", "*", "/",
		"^", "\", AND", "MOD", "OR", "XOR", "NOT", "x"
	};

	static char *Etokens1[] = {
		"ABS", "ASC", "ATN", "CHR$", "CINT", "COS", "CREAL", "EXP",
		"FIX", "FRE", "INKEY", "INP", "INT", "JOY", "LEN", "LOG",
		"LOG10", "LOWER$", "PEEK", "REMAIN", "SGN", "SIN", "SPACE$", "SQ",
		"SQR", "STR$", "TAN", "UNT", "UPPER$", "VAL"
	};

	static char *Etokens2[] = {
		"EOF", "ERR", "HIMEM", "INKEY$", "PI", "RND", "TIME", "XPOS", "YPOS", "DERR"
	};

	static char *Etokens3[] = {
		"BIN$", "DEC$", "HEX$", "ISNTR", "LEFT$", "MAX", "MIN", "POS", "RIGHT$", "ROUND", "STRING$", "TEST", "TESTR", "COPYCHR$", "VPOS"
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
	BYTE Suffix      = 0;
	WORD NumLit      = 0;

	DWORD FloatM     = 0;
	BYTE  FloatE     = 0;
	bool  ElseFudge  = false;

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
			case 1:	// line length
				if ( DigitCount == 0 )
				{
					BASLen = InChar;
				}
				else
				{
					BASLen |= (WORD) InChar << 8;
				}

				DigitCount++;

				if ( DigitCount == 2 )
				{
					DecodeState = 2;
					DigitCount  = 0;

					if ( BASLen == 0 )
					{
						FilePtr = FileSize;
					}
				}

				break;

			case 2: // Line number
				if ( DigitCount == 0 )
				{
					BASLine = InChar;
				}
				else
				{
					BASLine |= (WORD) InChar << 8;
				}

				DigitCount++;

				if ( DigitCount == 2 )
				{
					DecodeState = 3;
					DigitCount  = 0;
					BASLen      = 0;
					tokenSet    = tokens;
				}

				break;


			case 3:	// line data
				if ( InChar == 0x00)
				{
					/* Start of new line */
					rsprintf( &pOutBuffer[ lOutPtr ], "%*d %s", 5, BASLine, TranslatedLine );

					lOutPtr += rstrnlen( &pOutBuffer[ lOutPtr ], 1024 );

					DecodeState = 1;
					BASLine     = 0;
					DigitCount  = 0;

					opts->LinePointers.push_back( lOutPtr );

					TranslatedPtr = 0;
					IsQuoted      = false;
					IsKeyword     = false;

					::PostMessage( opts->ProgressWnd, WM_TEXT_PROGRESS, (WPARAM) Percent( 0, 1, FilePtr, FileSize, true ), 0 );
				}
				else if ( ( InChar == 0xFF ) && ( !IsQuoted ) )
				{
					DecodeState = 5;
				}
				else if ( ( InChar >= 0x02 ) && ( InChar <= 0x04 ) )
				{
					Suffix = 0;

					if ( InChar == 0x02 ) { Suffix = '%'; };
					if ( InChar == 0x03 ) { Suffix = '$'; };
					if ( InChar == 0x04 ) { Suffix = '!'; };

					DecodeState = 6;
					DigitCount  = 0;
				}
				else if ( ( InChar >= 0x0b ) && ( InChar <= 0x0d ) )
				{
					Suffix = 0;

					DecodeState = 6;
					DigitCount  = 0;
				}
				else if ( InChar == 0x1F )
				{
					DigitCount  = 0;
					DecodeState = 4;
					FloatM      = 0;
					FloatE      = 0;
				}
				else if ( ( InChar >= 0x19 ) && ( InChar <= 0x1E ) )
				{
					Suffix      = InChar;
					DecodeState = 7;
					DigitCount  = 0;
					NumLit      = 0;
				}
				else if ( InChar == 0x7C )
				{
					DecodeState = 8;
					DigitCount  = 0;
				}
				else
				{
					if ( InChar == '\"')
					{
						IsQuoted = !IsQuoted;
					}

					if ( ( InChar >= 0x80 ) && ( InChar <= 0xFF ) && ( !IsQuoted ) )
					{
						/* ELSE seems to always be prefixed with a colon */
						if ( ( ElseFudge ) && ( InChar == 0x97 ) )
						{
							TranslatedPtr--;
						}

						rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) tokens[ InChar - 0x80 ] );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						ElseFudge = false;
					}
					else if ( ( InChar >= 0x00 ) && ( InChar <= 0x04 ) && ( !IsQuoted ) )
					{
						rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) lowtokens[ InChar ] );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						if ( InChar == 0x01 ) { ElseFudge = true; }
					}
					else if ( ( InChar >= 0x0e ) && ( InChar <= 0x1F ) && ( !IsQuoted ) )
					{
						rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) lowtokens[ InChar ] );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );
					}
					else
					{
						TranslatedLine[ TranslatedPtr++ ] = InChar;
						TranslatedLine[ TranslatedPtr   ] = 0;
					}
				}				

				break;

			case 4: // Floating point value
				{
					DWORD v = (DWORD) InChar;

					if ( DigitCount < 4 )
					{
						FloatM |= ( v << ( 8 * DigitCount ) );
					}
					else
					{
						FloatE = InChar;
					}

					DigitCount++;

					if ( DigitCount == 5 )
					{
						float f = FloatFromMantissa( FloatM, FloatE );

						rsprintf( &TranslatedLine[ TranslatedPtr ], "%g", f );
						
						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						DecodeState = 3;
					}
				}
				break;

			case 5: // Extended Tokenised data
				if ( ( InChar >= 0x00 ) && ( InChar <= 0x1D ) && ( !IsQuoted ) )
				{
					rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) Etokens1[ InChar ] );
						
					TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );
				}
				else if ( ( InChar >= 0x40 ) && ( InChar <= 0x49 ) && ( !IsQuoted ) )
				{
					rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) Etokens2[ InChar - 0x40 ] );
						
					TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );
				}
				else if ( ( InChar >= 0x71 ) && ( InChar <= 0x7F ) && ( !IsQuoted ) )
				{
					rsprintf( &TranslatedLine[ TranslatedPtr ], "%s", (BYTE *) Etokens3[ InChar - 0x71 ] );
						
					TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

					tokenSet    = tokens;
					tokenOffset = 0x80;
				}
				else
				{
					TranslatedLine[ TranslatedPtr++ ] = InChar;
					TranslatedLine[ TranslatedPtr   ] = 0;
				}

				DecodeState = 3 ;

				break;

			case 6: // Variable name
				DigitCount++;

				if ( DigitCount >= 3 )
				{
					TranslatedLine[ TranslatedPtr++ ] = InChar & 0x7F;
					TranslatedLine[ TranslatedPtr   ] = 0;
				}

				if ( InChar & 0x80 )
				{
					DecodeState = 3;

					if ( Suffix != 0 )
					{
						TranslatedLine[ TranslatedPtr++ ] = Suffix;
						TranslatedLine[ TranslatedPtr   ] = 0;
					}
				}
				break;

			case 7: // Numeric literal
				{
					WORD v = (WORD) InChar;

					NumLit |= ( v << ( 8 * DigitCount ) );

					if ( Suffix == 0x19 ) { DigitCount++; }

					DigitCount++;

					if ( DigitCount == 2 )
					{
						switch ( Suffix )
						{
						case 0x19:
						case 0x1A:
						case 0x1D:
						case 0x1E:
							rsprintf( &TranslatedLine[ TranslatedPtr ], "%d", NumLit );
							break;

						case 0x1b:
							rsprintf( &TranslatedLine[ TranslatedPtr ], "&X%X", NumLit );
							break;

						case 0x1c:
							rsprintf( &TranslatedLine[ TranslatedPtr ], "&%X", NumLit );
							break;
						}

						TranslatedPtr += rstrnlen( &TranslatedLine[ TranslatedPtr ], 1024 );

						DecodeState =3;
					}
				}
				break;

			case 8: // RSX - e.g. |CPM or |DIR
				if ( DigitCount == 0 )
				{
					TranslatedLine[ TranslatedPtr++ ] = 0x7C;
					TranslatedLine[ TranslatedPtr   ] = 0;
				}
				else
				{
					TranslatedLine[ TranslatedPtr++ ] = InChar & 0x7F;
					TranslatedLine[ TranslatedPtr   ] = 0;

					if ( InChar & 0x80 )
					{
						DecodeState = 3;
					}
				}

				DigitCount++;

				break;
		}

		FilePtr++;
	}

	opts->EncodingID     = ENCODING_CPC;
	*opts->pTextBuffer   = pOutBuffer;
	opts->TextBodyLength = lOutPtr;

	return 0;
}

float LocomotiveBASICTranslator::FloatFromMantissa( DWORD Mantissa, BYTE Exponent )
{
	bool neg = false;

	if ( Mantissa & 0x80 )
	{
		Mantissa &= 0xFFFFFF7F;

		neg = true;
	}

	/* Because $REASONS */
	Mantissa |= 0x80000000;

	float n = (float) Mantissa;

	if ( neg )
	{
		n = 0 - n;
	}

	int   p = (int) Exponent;

	/* Remove bias */
	p = p - 128;
	p = 0 - p;

	n *= (float) pow( (double) 2, p );

	return n;
}