#include "StdAfx.h"
#include "TextFileTranslator.h"


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

	bool CRLF = false;

	opts->LinePointers.clear();

	while ( TextFilePtr < TextFileLen )
	{
		DWORD ReadSize = (DWORD) ( TextFileLen - TextFilePtr );

		if ( ReadSize > 1024 ) { ReadSize = 1024; }

		FileObj.Read( LineBuf, ReadSize );

		for ( DWORD i=0; i<ReadSize; i++ )
		{
			if ( ( LineBuf[ i ] == 0xA ) || ( LineBuf[ i ] == 0x0D ) )
			{
				if ( !CRLF )
				{
					opts->LinePointers.push_back( TextBodyPtr );

					CRLF = true;
				}
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

				CRLF = false;
			}
		}

		TextFilePtr += ReadSize;
	}

	*opts->pTextBuffer   = pTextBody;
	opts->TextBodyLength = TextFileLen;

	return 0;
}
