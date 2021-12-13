#include "StdAfx.h"
#include "ARMadeusTranslator.h"


ARMadeusTranslator::ARMadeusTranslator(void)
{
}


ARMadeusTranslator::~ARMadeusTranslator(void)
{
}

#define CONVERSION_FACTOR 5.5125

// ARMadeus files are basically straight, 8-bit mono 8kHz audio files.
// We'll convert this by working through the 44kHz buffer, and picking the 8kHz
// sample that represents the 44kHz point.
int ARMadeusTranslator::Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx )
{
	tx->CuePoints.clear();
	tx->FormatName = L"ARMadeus";
	tx->Stereo     = false;
	tx->WideBits   = false;

	tx->Title.UseEncoding = false;
	tx->Title.Name        = L"ARMadeus Sample";

	BYTE Buffer[ 32768 ];
	BYTE OutBuffer[ 44100 ];
	
	QWORD SeekPoint = 0;
	QWORD SeekEnd   = 32768;
	DWORD OutStart  = 0;
	DWORD OutEnd    = 44100;

	obj.Seek( SeekPoint );
	Output.Seek( 0 );

	obj.Read( Buffer, 32768 );

	DWORD Length8 = (DWORD) obj.Ext();

	DWORD Samples44 = (DWORD) ( (double) Length8 * CONVERSION_FACTOR );

	BYTE Sample;

	for ( DWORD i=0; i<Samples44; i++ )
	{
		if ( i >= OutEnd )
		{
			OutStart += 44100;
			OutEnd   += 44100;

			Output.Write( OutBuffer, 44100 );
		}

		if ( WaitForSingleObject( tx->hStopTranslating, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		QWORD Position8 = (QWORD) ( (double) i / CONVERSION_FACTOR );

		if ( Position8 >= SeekEnd )
		{
			SeekPoint += 32768;
			SeekEnd   += 32768;

			obj.Read( Buffer, 32768 );
		}

		DWORD BufferPos = (DWORD) ( Position8 - SeekPoint );

		Sample = Buffer[ BufferPos ];

		// ARMadeus uses signed 8-bit, we use unsigned with a 127 centre.
		BYTE OutSample = 127;

		if ( Sample < 128 )
		{
			OutSample += Sample;
		}
		else
		{
			// We need to cap this
			if ( Sample > 0x81 )
			{
				OutSample -= ( 0xFF - Sample ) + 1;
			}
			else
			{
				OutSample = 0;
			}
		}

		OutBuffer[ i - OutStart ] = OutSample;
	}

	if ( Samples44 > OutStart )
	{
		Output.Write( OutBuffer, Samples44 - OutStart );
	}

	tx->AudioSize = Samples44;

	return 0;
}