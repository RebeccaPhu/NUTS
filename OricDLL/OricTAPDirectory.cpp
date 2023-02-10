#include "StdAfx.h"
#include "OricTAPDirectory.h"

#include "..\NUTS\NUTSError.h"
#include "..\NUTS\libfuncs.h"
#include "OricDefs.h"

OricTAPDirectory::OricTAPDirectory( DataSource *pSrc ) : Directory( pSrc )
{
}


OricTAPDirectory::~OricTAPDirectory(void)
{
}

int OricTAPDirectory::ReadDirectory( void )
{
	Files.clear();

	QWORD Offset = 0;

	QWORD Target = pSource->PhysicalDiskSize;

	BYTE Buffer[ 128 ];

	DWORD FileID = 0;

	while ( Offset < Target )
	{
		NativeFile file;

		if ( pSource->ReadRaw( Offset, 128, Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		BYTE p = 0;

		while ( ( Buffer[ p ] == 0x16 ) && ( p < 100 ) )
		{
			p++;
		}

		if ( p == 100 )
		{
			return NUTSError( 0xC04, L"Unexpected run of sync bytes" );
		}

		if ( Buffer[ p ] != 0x24 )
		{
			return NUTSError( 0xC05, L"Sync marker not found" );
		}

		// More than 16 sync bytes means "fast" encoding.
		if ( p >= 16 ) { file.Attributes[ 0 ] = 0xFFFFFFFF; } else { file.Attributes[ 0 ] = 0x00000000; }

		p++;

		file.Attributes[ 1 ] = BETWORD( &Buffer[ p ] ); // File type byte

		// Do some fix up here. For BASIC/MCode, first two bytes can be anything.
		if ( ( file.Attributes[ 1 ] & 0x000000FF ) != 0x40 )
		{
			file.Attributes[ 1 ] &= 0x000000FF;
		}

		p += 3;

		file.Attributes[ 2 ] = Buffer[ p++ ]; // Autorun byte

		// Don't store the file end address, as we'll calculate it. But it gives us the length
		WORD EndAddr = BEWORD( &Buffer[ p ] ); p += 2;

		file.Attributes[ 3 ] = BEWORD( &Buffer[ p ]  ); p += 2; // 

		file.Length = ( EndAddr - file.Attributes[ 3 ] ) + 1;

		// Skip the last unused byte.
		p++;

		WORD fnl = rstrnlen( &Buffer[ p ], 15 );

		file.Filename = BYTEString( &Buffer[ p ], fnl );

		p += fnl + 1;

		file.Attributes[ 4 ] = DWORD( Offset ) + p; // Raw offset

		Offset += p + file.Length;

		// Fill in the rest of the data
		file.EncodingID = ENCODING_ORIC;
		file.fileID     = FileID;
		file.Flags      = FF_Audio;
		file.FSFileType = FT_ORIC;

		switch ( file.Attributes[ 1 ] )
		{
			case ORIC_TAP_FILE_BASIC:
				file.Type = FT_BASIC;
				file.Icon = FT_BASIC;
				break;

			case ORIC_TAP_FILE_MCODE:
				file.Type = FT_Code;
				file.Icon = FT_Code;
				break;

			case ORIC_TAP_FILE_AR_IN:
			case ORIC_TAP_FILE_AR_RL:
			case ORIC_TAP_FILE_AR_ST:
				file.Type = FT_Data;
				file.Icon = FT_Data;
				break;
		
			default:
				break;
		}

		Files.push_back( file );

		FileID++;
	}

	return 0;
}

int OricTAPDirectory::WriteDirectory( void )
{
	return 0;
}

