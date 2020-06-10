#include "stdafx.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "ZIPFile.h"
#include "ZipFuncs.h"

#include "archive.h"
#include "archive_entry.h"

BYTE *ZIPFile::GetTitleString( NativeFile *pFile )
{
	static BYTE Title[ 384 ];

	rsprintf( Title, "ZIP::/%s", cpath );

	WORD l = rstrnlen( Title, 384 );

	Title[ l - 1 ] = 0;

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

int ZIPFile::ReadFile(DWORD FileID, CTempFile &store)
{
	struct archive *a;
	struct archive_entry *entry;

	int r;

	DWORD fileID = 0;
	DWORD SeqID  = pDirectory->Files[ FileID ].Attributes[ 0 ];

	a = archive_read_new();

	archive_read_support_filter_all( a );
	archive_read_support_format_all( a );
	archive_read_support_compression_all(a);

	r = archive_read_open_filename (a, pSource->GetLocation(), 10240 );

	if ( r != ARCHIVE_OK )
	{
		return NUTSError( 0xA0, ZIPError( a ) );
	}

	while ( ( r = archive_read_next_header( a, &entry ) ) == ARCHIVE_OK )
	{
		if ( fileID == SeqID )
		{
			store.Seek( 0 );

			DWORD BytesToGo = archive_entry_size( entry );

			BYTE Buffer[ 10240 ];

			while ( BytesToGo > 0 )
			{
				DWORD BytesRead = BytesToGo;

				if ( BytesRead > 10240 ) { BytesRead = 10240; }

				r = archive_read_data( a, Buffer, BytesRead );

				if ( ( r == ARCHIVE_OK ) || ( r == ARCHIVE_WARN ) )
				{
					store.Write( Buffer, BytesRead );

					BytesToGo -= BytesRead;
				}
				else if ( ( r == ARCHIVE_FATAL ) || ( r == ARCHIVE_FAILED ) )
				{
					int r = NUTSError( 0xA1, ZIPError( a ) );

					archive_read_free( a );

					return r;
				}
			}
		}

		fileID++;
	}

	r = archive_read_free( a );

	if ( r != ARCHIVE_OK )
	{
		return NUTSError( 0xA0, ZIPError( a ) );
	}

	return 0;
}

int ZIPFile::ChangeDirectory( DWORD FileID )
{
	rstrncat( cpath, pDirectory->Files[ FileID ].Filename, 255 );
	rstrncat( cpath, (BYTE *) "/", 255 );

	rstrncpy( pDir->cpath, cpath, 255 );

	return pDirectory->ReadDirectory();
}

int ZIPFile::Parent()
{
	BYTE *p = rstrrchr( cpath, '/', 255 );

	if ( p != nullptr )
	{
		*p = 0;
	}

	p = rstrrchr( cpath, '/', 255 );

	if ( p == nullptr )
	{
		cpath[ 0 ] = 0;
	}
	else
	{
		p++;

		*p = 0;
	}

	rstrncpy( pDir->cpath, cpath, 255 );

	return pDirectory->ReadDirectory();
}

bool ZIPFile::IsRoot()
{
	if ( cpath[ 0 ] == 0 )
	{
		return true;
	}

	return false;
}