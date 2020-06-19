#include "stdafx.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "ZIPFile.h"
#include "ZipFuncs.h"
#include "Plugins.h"

#include "ZIPCommon.h"

#include "archive.h"
#include "archive_entry.h"
#include "zip.h"

BYTE *ZIPFile::GetTitleString( NativeFile *pFile )
{
	static BYTE Title[ 384 ];

	rsprintf( Title, "ZIP::/%s", cpath );

	WORD l = rstrnlen( Title, 384 );

	if ( l > 6 )
	{
		Title[ l - 1 ] = 0;
	}

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
	zip_error_t ze;

	zip_error_init( &ze );

	DWORD fileID = 0;
	DWORD SeqID  = pDirectory->Files[ FileID ].Attributes[ 0 ];

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	store.Seek( 0 );

	zip_stat_t zs;
	zip_stat_init( &zs );
	zip_stat_index( za, SeqID, NULL, &zs );

	DWORD BytesToGo = zs.size;

	BYTE Buffer[ 10240 ];

	zip_file_t *fd = zip_fopen_index( za, SeqID, NULL );

	if (fd == nullptr )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = BytesToGo;

		if ( BytesRead > 10240 ) { BytesRead = 10240; }

		zip_int64_t r = zip_fread( fd, Buffer, 10240 );

		if ( r < 0 )
		{
			return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
		}

		store.Write( Buffer, BytesRead );

		BytesToGo -= BytesRead;
	}

	zip_fclose( fd );

	zip_error_fini( &ze );
	zip_source_close( z );
	zip_source_free( z );

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

int ZIPFile::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( pFile->EncodingID != ENCODING_ASCII )
	{
		/* ONLY ASCII accepted */
		return FILEOP_NEEDS_ASCII;
	}

	/* Existence check */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( &*iFile, pFile ) )
		{
			return FILEOP_EXISTS;
		}
	}

	BYTE field[ 64 ];
	BYTE fname[ MAX_PATH ];

	/* Try and get this translated first */
	bool DidTranslate = FSPlugins.TranslateZIPContent( pFile, field );

	zip_error_t ze;

	zip_error_init( &ze );

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	store.Seek( 0 );

	/* libzip wants a source made from the data. Well alrighty then. */
	ZIPFromTemp *zft = new ZIPFromTemp( store );

	zip_source_t *zs = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) zft, &ze );

	rstrncpy( fname, cpath, MAX_PATH );
	rstrncat( fname, pFile->Filename, MAX_PATH );

	zip_uint64_t zfi = zip_file_add( za, (char *) fname, zs, ZIP_FL_ENC_CP437 | ZIP_FL_OVERWRITE );

	if ( DidTranslate )
	{
		zip_flags_t attrSource = ZIP_FL_LOCAL;

		DWORD NumFields = zip_file_extra_fields_count( za, zfi, attrSource );

		if ( NumFields == 0xFFFFFFFF )
		{
			attrSource = ZIP_FL_CENTRAL;

			NumFields = zip_file_extra_fields_count( za, zfi, attrSource );
		}

		if ( NumFields == 0xFFFFFFFF )
		{
			NumFields = 0;
		}

		zip_file_extra_field_set( za, zfi, * ( zip_uint16_t *) &field[ 0 ], ( zip_uint16_t ) NumFields, (zip_uint8_t *) &field[4], (zip_uint16_t) * (WORD *) &field[ 2 ], ZIP_FL_LOCAL );
		zip_file_extra_field_set( za, zfi, * ( zip_uint16_t *) &field[ 0 ], ( zip_uint16_t ) NumFields, (zip_uint8_t *) &field[4], (zip_uint16_t) * (WORD *) &field[ 2 ], ZIP_FL_CENTRAL );
	}

	zip_error_fini( &ze );
	zip_close( za );

	delete zft;

	return pDirectory->ReadDirectory();
}

int ZIPFile::DeleteFile( NativeFile *pFile, int FileOp )
{
	DWORD SeqID = 0xFFFFFFFF;

	if ( FileOp == FILEOP_COPY_FILE )
	{
		if ( pFile->EncodingID != ENCODING_ASCII )
		{
			/* ONLY ASCII accepted */
			return FILEOP_NEEDS_ASCII;
		}
	}

	/* Existence check */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( &*iFile, pFile ) )
		{
			SeqID = iFile->Attributes[ 0 ];

			pFile = &*iFile;

			break;
		}
	}

	if ( iFile == pDirectory->Files.end() )
	{
		return 0;
	}

	zip_error_t ze;

	zip_error_init( &ze );

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	zip_delete( za, SeqID );

	zip_error_fini( &ze );
	zip_close( za );

	return pDirectory->ReadDirectory();
}

int	ZIPFile::CreateDirectory( BYTE *Filename, bool EnterAfter )
{
	/* Existence check */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( rstrnicmp( iFile->Filename, Filename, 255 ) )
		{
			return FILEOP_EXISTS;
		}
	}

	BYTE fname[ MAX_PATH ];

	zip_error_t ze;

	zip_error_init( &ze );

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	rstrncpy( fname, cpath, MAX_PATH );
	rstrncat( fname, Filename, MAX_PATH );

	zip_uint64_t zfi = zip_dir_add( za, (char *) fname, ZIP_FL_ENC_CP437 );

	zip_error_fini( &ze );
	zip_close( za );

	if ( EnterAfter )
	{
		rstrncat( cpath, Filename, 255 );
		rstrncat( cpath, (BYTE *) "/", 255 );

		rstrncpy( pDir->cpath, cpath, 255 );
	}

	return pDirectory->ReadDirectory();
}

int ZIPFile::Rename( DWORD FileID, BYTE *NewName )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE fname[ MAX_PATH ];

	rstrncpy( fname, cpath, MAX_PATH );
	rstrncat( fname, NewName, MAX_PATH );

	DWORD SeqID = pFile->Attributes[ 0 ];

	zip_error_t ze;

	zip_error_init( &ze );

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	if ( ! ( pFile->Flags & FF_Directory ) )
	{
		zip_file_rename( za, SeqID, (char *) fname, ZIP_FL_ENC_CP437 );
	}

	zip_error_fini( &ze );
	zip_close( za );

	return pDirectory->ReadDirectory();
}

int ZIPFile::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	BYTE field[ 64 ];
	BYTE fname[ MAX_PATH ];

	/* Try and get this translated first */
	bool DidTranslate = FSPlugins.TranslateZIPContent( pFile, field );

	zip_error_t ze;

	zip_error_init( &ze );

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	store.Seek( 0 );

	/* libzip wants a source made from the data. Well alrighty then. */
	ZIPFromTemp *zft = new ZIPFromTemp( store );

	zip_source_t *zs = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) zft, &ze );

	rstrncpy( fname, cpath, MAX_PATH );
	rstrncat( fname, pFile->Filename, MAX_PATH );

	DWORD SeqID = pFile->Attributes[ 0 ];

	if ( zip_file_replace( za, SeqID, zs, ZIP_FL_ENC_CP437 | ZIP_FL_OVERWRITE ) != 0 )
	{
		return NUTSError( 0xA2, L"ZIP Error" );
	}

	if ( DidTranslate )
	{
		zip_flags_t attrSource = ZIP_FL_LOCAL;

		DWORD NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );

		if ( NumFields == 0xFFFFFFFF )
		{
			attrSource = ZIP_FL_CENTRAL;

			NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );
		}

		if ( NumFields == 0xFFFFFFFF )
		{
			NumFields = 0;
		}

		zip_file_extra_field_set( za, SeqID, * ( zip_uint16_t *) &field[ 0 ], ( zip_uint16_t ) NumFields, (zip_uint8_t *) &field[4], (zip_uint16_t) * (WORD *) &field[ 2 ], ZIP_FL_LOCAL );
		zip_file_extra_field_set( za, SeqID, * ( zip_uint16_t *) &field[ 0 ], ( zip_uint16_t ) NumFields, (zip_uint8_t *) &field[4], (zip_uint16_t) * (WORD *) &field[ 2 ], ZIP_FL_CENTRAL );
	}

	zip_error_fini( &ze );
	zip_close( za );

	delete zft;

	/* Update the source file */
	pFile->Length          = pSource->PhysicalDiskSize;
	
	return 0;
}