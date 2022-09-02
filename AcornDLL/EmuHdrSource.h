#pragma once
#include "..\nuts\offsetdatasource.h"
#include "..\NUTS\NUTSError.h"
#include "..\NUTS\NUTSFlags.h"

class EmuHdrSource :
	public OffsetDataSource
{
public:
	EmuHdrSource( DataSource *pSource );
	~EmuHdrSource(void);

public:
	int PrepareFormat()
	{
		// Formats the first 0x200 bytes as 0xFF so that the underlying garbage
		// isn't misdetected later on.

		BYTE Buffer[ 0x200 ];

		memset( Buffer, 0xFF, 0x200 );

		int r;

		if ( pSrc != nullptr )
		{
			r = pSrc->WriteRaw( 0, 0x200, Buffer );
		}
		else
		{
			r = NUTSError( 0xA02, L"Data source error" );
		}

		return r;
	}

	bool Valid()
	{
		if ( pSrc->Flags & DS_RawDevice )
		{
			return false;
		}

		// Do some basic checking
		BYTE Buffer[ 0x400 ];

		// We need to examine the beginning, because the image might co-incidentally contain a directory at the right place for an offset

		// 256-byte formats.
		pSrc->ReadRaw( 0x200, 0x100, Buffer );

		// Look for Hugo, Nick, or SBPr at 0x200 bytes in - this is the root dir of old map FSes.
		if ( memcmp( &Buffer[ 1 ], (void *) "Hugo", 4 ) == 0 ) { return false; }
		if ( memcmp( &Buffer[ 1 ], (void *) "Nick", 4 ) == 0 ) { return false; }
		if ( memcmp( &Buffer[ 1 ], (void *) "SBPr", 4 ) == 0 ) { return false; }

		// D Format will have one of the above at fake sector 4, real sector 4.
		pSrc->ReadRaw( 0x400, 0x100, Buffer );

		if ( memcmp( &Buffer[ 1 ], (void *) "Hugo", 4 ) == 0 ) { return false; }
		if ( memcmp( &Buffer[ 1 ], (void *) "Nick", 4 ) == 0 ) { return false; }
		if ( memcmp( &Buffer[ 1 ], (void *) "SBPr", 4 ) == 0 ) { return false; }

		// Now we can look with the offset, and if we find what we're looking for here, it confirms our suspicions.

		// 256-byte formats.
		pSrc->ReadRaw( 0x200 + 0x200, 0x100, Buffer );

		// Look for Hugo, Nick, or SBPr at 0x200 bytes in - this is the root dir of old map FSes.
		if ( memcmp( &Buffer[ 1 ], (void *) "Hugo", 4 ) == 0 ) { return true; }
		if ( memcmp( &Buffer[ 1 ], (void *) "Nick", 4 ) == 0 ) { return true; }
		if ( memcmp( &Buffer[ 1 ], (void *) "SBPr", 4 ) == 0 ) { return true; }

		// D Format will have one of the above at fake sector 4, real sector 4.
		pSrc->ReadRaw( 0x400 + 0x200, 0x100, Buffer );

		if ( memcmp( &Buffer[ 1 ], (void *) "Hugo", 4 ) == 0 ) { return true; }
		if ( memcmp( &Buffer[ 1 ], (void *) "Nick", 4 ) == 0 ) { return true; }
		if ( memcmp( &Buffer[ 1 ], (void *) "SBPr", 4 ) == 0 ) { return true; }

		// OK, not an old map. We can do some flimsy checks for new map that will have some confidence.
		pSrc->ReadRaw( 0x200, 0x100, Buffer );

		// Check the parameters are sane in the correct location.
		if ( ( Buffer[ 0x08 ] < 22 ) && ( Buffer[ 0x0D ] > 0 ) )
		{
			// Looks like we found a floppy new map without an offset. Call it invalid.
			return false;
		}

		// Try with the offset now
		pSrc->ReadRaw( 0x200 + 0x200, 0x100, Buffer );

		if ( ( Buffer[ 0x08 ] < 22 ) && ( Buffer[ 0x0D ] > 0 ) )
		{
			// Looks like we found a floppy new map WITH an offset. Weird, but OK.
			return true;
		}

		// Fine. This could be a hard disk image though, and so the structures could be
		// bloody anywhere. (Acorn, why)

		// Read the boot block snippet without the offset first.
		pSrc->ReadRaw( 0xC00, 0x1D0, Buffer );

		// Check the parameters are sane in the correct location.
		if ( ( Buffer[ 0x1C4 ] < 22 ) && ( Buffer[ 0x1C9 ] > 0 ) )
		{
			// Looks like we found a hard disk new map without an offset. Call it invalid.
			return false;
		}

		// Now read the boot block snippet WITH the offset, and see if it looks right.
		pSrc->ReadRaw( 0xC00 + 0x200, 0x1D0, Buffer );

		// Check the parameters are sane in the correct location.
		if ( ( Buffer[ 0x1C4 ] < 22 ) && ( Buffer[ 0x1C9 ] > 0 ) )
		{
			// Looks like we found a hard disk new map WITH an offset. BINGO!
			return true;
		}

		// No frigging idea. But it doesn't look valid, so return false.

		return false;
	}
};

