#include "StdAfx.h"
#include "SpriteFile.h"
#include "RISCOSIcons.h"
#include "Sprite.h"
#include "../nuts/libfuncs.h"

#include "../NUTS/SourceFunctions.h"

FSHint SpriteFile::Offer( BYTE *Extension )
{
	FSHint hint;

	/* There really are no clues on this one :( */
	hint.Confidence = 0;
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

int SpriteFile::WriteFile(NativeFile *pFile, CTempFile &store)
{
	CTempFile OutSpriteFile;

	/* Write out a header first - We'll fill this in as we go, then rewrite it after */
	DWORD Header[ 4 ];

	OutSpriteFile.Seek( 0 );

	OutSpriteFile.Write( Header, 16 );

	/* Write out the list of files. We need to store the start of each file temporarily along with the position to write it in the file */
	std::map< DWORD, QWORD> SpriteOffsetPositions;
	std::map< DWORD, DWORD> SpriteSizes;

	DWORD OffsetPtr = 16;

	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		SpriteSizes[ iFile->fileID ] = iFile->Length;

		while ( SpriteSizes[ iFile->fileID ] % 4 ) { SpriteSizes[ iFile->fileID ]++; }

		SpriteOffsetPositions[ iFile->fileID ] = OffsetPtr;

		CTempFile sprite;

		ReadFile( iFile->fileID, sprite );

		CopyContent( sprite, OutSpriteFile );

		OffsetPtr += SpriteSizes[ iFile->fileID ];
	}

	/* Add on the new sprite */
	DWORD NewIndex = pDirectory->Files.size();

	store.Seek( 0 );

	SpriteSizes[ NewIndex ] = store.Ext();

	while ( SpriteSizes[ NewIndex ] % 4 ) { SpriteSizes[ NewIndex ]++; }

	SpriteOffsetPositions[ NewIndex ] = OffsetPtr;

	CTempFile sprite;

	CopyContent( store, OutSpriteFile );

	OffsetPtr += SpriteSizes[ NewIndex ];

	/* Fill in the header */
	Header[ 0 ] = pDirectory->Files.size() + 1;
	Header[ 1 ] = 20; /* Extra four bytes to account for missing sprite area size field */
	Header[ 2 ] = OffsetPtr;
	Header[ 3 ] = 0;

	OutSpriteFile.Seek( 0 );
	OutSpriteFile.Write( Header, 16 );

	/* Go back and write the offsets */
	std::map< DWORD, QWORD >::iterator iSprite;

	for ( iSprite = SpriteOffsetPositions.begin(); iSprite != SpriteOffsetPositions.end(); iSprite++ )
	{
		OutSpriteFile.Seek( iSprite->second );

		OutSpriteFile.Write( &SpriteSizes[ iSprite->first ], 4 );
	}

	/* Now replace the entire source */
	ReplaceSourceContent( pSource, OutSpriteFile );

	FreeIcons( );

	int r = pDirectory->ReadDirectory();

	ResolveIcons( );

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

int SpriteFile::FreeIcons( void )
{
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( iFile->HasResolvedIcon )
		{
			if ( pDirectory->ResolvedIcons[ iFile->fileID ].pImage != nullptr )
			{
				free( pDirectory->ResolvedIcons[ iFile->fileID ].pImage );
			}
		}

		iFile->HasResolvedIcon = false;
	}

	pDirectory->ResolvedIcons.clear();

	return 0;
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

int SpriteFile::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
{
	DWORD Offset = (DWORD) pDirectory->Files[ FileID ].Attributes[ 0 ];

	BYTE SpriteName[ 12 ];

	/* Update our directory's copy */
	pDirectory->Files[ FileID ].Filename = BYTEString( NewName, 12 );

	/* Update the file itself */
	rstrncpy( SpriteName, NewName, 12 );

	pSource->WriteRaw( Offset + 4, 12, SpriteName );

	/* Re-read the directory to update */
	FreeIcons();

	int r = pDirectory->ReadDirectory();

	ResolveIcons();

	return r;
}

AttrDescriptors SpriteFile::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start offset. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"File offset";
	Attrs.push_back( Attr );

	/* Mode */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile;
	Attr.Name  = L"Mode";
	Attrs.push_back( Attr );

	/* Width */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile;
	Attr.Name  = L"Width";
	Attrs.push_back( Attr );

	/* Height */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile;
	Attr.Name  = L"Height";
	Attrs.push_back( Attr );

	/* BPP */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile;
	Attr.Name  = L"Bits per pixel";
	Attrs.push_back( Attr );


	return Attrs;
}

/* Simplest formatter ever */
int SpriteFile::Format_Process( FormatType FT, HWND hWnd ) {
	DWORD Header[ 4 ] = { 0, 20, 20, 0 };

	int r = pSource->WriteRaw( 0, 16, (BYTE *) Header );

	PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

	/* No seriously, that's it. It creates a basic header that says "No sprites here". */

	return r;
}

