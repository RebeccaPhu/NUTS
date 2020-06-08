#include "stdafx.h"

#include "ZIPFile.h"

#include "archive.h"

BYTE *ZIPFile::GetTitleString( NativeFile *pFile )
{
	static BYTE Title[ 384 ];

	rsprintf( Title, "ZIP::%s", cpath );

	if ( pFile != nullptr )
	{
		BYTE chevron[4] = { "   " };

		chevron[ 1 ] = 175;

		rstrncat( Title, (BYTE *) "/", 384 );
		rstrncat( Title, pFile->Filename,384 );
		rstrncat( Title, chevron, 384 );
	}

	return Title;
}

BYTE *ZIPFile::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE Status[ 384 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( Status, "%d file objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( Status, "%d file objects selected", SelectedItems );
	}
	else
	{
		rsprintf( Status, "%s, %d bytes", pDirectory->Files[ FileIndex ].Filename, pDirectory->Files[ FileIndex ].Length );
	}

	return Status;
}

BYTE *ZIPFile::DescribeFile( DWORD FileIndex )
{
	static BYTE Status[ 384 ];

	rsprintf( Status, "%d bytes", pDirectory->Files[ FileIndex ].Length );

	return Status;
}

