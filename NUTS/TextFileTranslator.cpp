#include "StdAfx.h"
#include "TextFileTranslator.h"
#include "libfuncs.h"

#include "NUTSMessages.h"

TextFileTranslator::TextFileTranslator(void)
{
}


TextFileTranslator::~TextFileTranslator(void)
{
}

int TextFileTranslator::TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
{
	BYTE *pTextBody   = nullptr; // Memory buffer
	DWORD TextBodySz  = 0;       // Size of memory buffer
	DWORD TextBodyLen = 0;       // Actual length of processed data
	DWORD TextBodyPtr = 0;       // Current processing pointer in text body

	QWORD TextFileLen = FileObj.Ext(); // Size of text file
	QWORD TextFilePtr = 0;             // Current processing pointer in text file

	FileObj.Seek( 0 );

	BYTE LineBuf[ 1024 ];
	BYTE LastCRLF = 0;

	opts->LinePointers.clear();

	while ( TextFilePtr < TextFileLen )
	{
		if ( WaitForSingleObject( opts->hStop, 0 ) == WAIT_OBJECT_0 )
		{
			/* ABORT! */
			*opts->pTextBuffer   = pTextBody;
			opts->TextBodyLength = TextBodySz;

			return 0;
		}

		DWORD ReadSize = (DWORD) ( TextFileLen - TextFilePtr );

		if ( ReadSize > 1024 ) { ReadSize = 1024; }

		FileObj.Read( LineBuf, ReadSize );

		for ( DWORD i=0; i<ReadSize; i++ )
		{
			if ( ( LineBuf[ i ] == 0xA ) || ( LineBuf[ i ] == 0x0D ) )
			{
				if ( ( LineBuf[ i ] == LastCRLF ) || ( LastCRLF == 0 ) )
				{
					opts->LinePointers.push_back( TextBodyPtr );

					::PostMessage( opts->ProgressWnd, WM_TEXT_PROGRESS, (WPARAM) Percent( 0, 1, TextFilePtr, TextFileLen, true ), 0 );
				}

				LastCRLF = LineBuf[ i ];
			}
			else
			{
				TextBodyLen++;

				while ( TextBodySz <= TextBodyLen )
				{
					TextBodySz += 1024;

					pTextBody = (BYTE *) realloc( pTextBody, TextBodySz );
				}

				pTextBody[ TextBodyPtr ] = LineBuf[ i ];

				TextBodyPtr++;

				LastCRLF = 0;
			}
		}

		TextFilePtr += ReadSize;
	}

	*opts->pTextBuffer   = pTextBody;
	opts->TextBodyLength = TextFileLen;

	return 0;
}
