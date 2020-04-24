#include "StdAfx.h"
#include "SpriteFile.h"
#include "RISCOSIcons.h"
#include "Sprite.h"
#include "../nuts/libfuncs.h"

FSHint SpriteFile::Offer( BYTE *Extension )
{
	FSHint hint;

	/* There really are no clues on this one :( */
	hint.Confidence = 1;
	hint.FSID       = FSID_SPRITE;

	return hint;
}

int	SpriteFile::ReadFile(DWORD FileID, CTempFile &store)
{
	store.Seek( 0 );

	DWORD FileSize = (DWORD) pDirectory->Files[ FileID ].Length;
	DWORD Offset   = (DWORD) pDirectory->Files[ FileID ].Attributes[ 0 ];

	while ( FileSize > 0 )
	{
		BYTE SpriteData[ 1024 ];

		DWORD Bytes = FileSize;

		if ( Bytes > 1024 ) { Bytes = 1024; }

		pSource->ReadRaw( Offset, Bytes, SpriteData );

		store.Write( SpriteData, Bytes );

		FileSize -= Bytes;
		Offset   += 1024;
	}

	return 0;
}


BYTE *SpriteFile::DescribeFile(DWORD FileIndex) {
	static BYTE status[ 64 ];

	rsprintf( status, "Sprite, %08X bytes", (DWORD) pDirectory->Files[ FileIndex ].Length );
		
	return status;
}

BYTE *SpriteFile::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE status[ 64 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d Sprites", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Sprites Selected", SelectedItems );
	}
	else 
	{
		rsprintf( status, "Sprite, %08X bytes", (DWORD) pDirectory->Files[ FileIndex ].Length );
	}
		
	return status;
}

BYTE *SpriteFile::GetTitleString( NativeFile *pFile )
{
	static BYTE ADFSPath[512];

	rsprintf( ADFSPath, "SpriteFS::$" );

	return ADFSPath;
}

int SpriteFile::ResolveIcons( void )
{
	if ( !UseResolvedIcons )
	{
		return 0;
	}

	NativeFileIterator iFile;

	DWORD MaskColour = GetSysColor( COLOR_WINDOW );	

	for (iFile=pDirectory->Files.begin(); iFile!=pDirectory->Files.end(); iFile++)
	{
		if ( iFile->HasResolvedIcon )
		{
			CTempFile FileObj;

			ReadFile( iFile->fileID, FileObj );

			Sprite sprite( FileObj );

			IconDef icon;

			sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

			icon.Aspect = sprite.SpriteAspect;

			pDirectory->ResolvedIcons[ iFile->fileID ] = icon;
		}
	}

	return 0;
}
