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

BYTE *ZIPFile::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE Title[ 384 ];

	rsprintf( Title, "ZIP::/%s", cpath );

	WORD l = rstrnlen( Title, 384 );

	if ( l > 6 )
	{
		Title[ l - 1 ] = 0;
	}

	BYTE chevron[4] = { "   " };

	if ( pFile != nullptr )
	{
		chevron[ 1 ] = 175;

		rstrncat( Title, (BYTE *) "/", 384 );
		rstrncat( Title, (BYTE *) pFile->Filename, 384 );

		if ( pFile->Flags & FF_Extension )
		{
			rstrncat( Title, (BYTE *) ".", 384 );
			rstrncat( Title, (BYTE *) pFile->Extension, 384 );
		}
	}

	if ( Flags & TF_Titlebar )
	{
		if ( !(Flags & TF_Final ) )
		{
			rstrncat( Title, chevron, 384 );
		}
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
		rsprintf( Status, "%s, %d bytes", (BYTE *) pDirectory->Files[ FileIndex ].Filename, pDirectory->Files[ FileIndex ].Length );
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

	rstrncpy( pZDir->cpath, cpath, 255 );

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

	rstrncpy( pZDir->cpath, cpath, 255 );

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
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ZIPATTR;
	fop.Direction = FOP_WriteEntry;
	fop.pXAttr    = field;
	fop.lXAttr    = 64;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;

	bool DidTranslate = ProcessFOP( &fop );

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

	if ( pFile->Flags & FF_Extension )
	{
		rstrncat( fname, (BYTE *) ".", MAX_PATH );
		rstrncat( fname, pFile->Extension, MAX_PATH );
	}

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

int ZIPFile::DeleteFile( DWORD FileID )
{
	DWORD SeqID = 0xFFFFFFFF;

	/* Existence check */
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	SeqID = pFile->Attributes[ 0 ];

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

int	ZIPFile::CreateDirectory( NativeFile *pDir, DWORD CreateFlags )
{
	/* Existence check */
	NativeFileIterator iFile;

	NativeFile SourceDir = *pDir;

	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( rstrnicmp( iFile->Filename, SourceDir.Filename, 10 ) )
		{
			if ( ( CreateFlags & ( CDF_MERGE_DIR | CDF_RENAME_DIR ) ) == 0 )
			{
				if ( iFile->Flags & FF_Directory )
				{
					return FILEOP_EXISTS;
				}
				else
				{
					return FILEOP_ISFILE;
				}
			}
			else if ( CreateFlags & CDF_MERGE_DIR )
			{
				return ChangeDirectory( iFile->fileID );
			}
			else if ( CreateFlags & CDF_RENAME_DIR )
			{
				if ( RenameIncomingDirectory( &SourceDir, pDirectory ) != NUTS_SUCCESS )
				{
					return -1;
				}
			}
		}
	}

	pDir = &SourceDir;

	BYTE field[ 64 ];

	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ZIPATTR;
	fop.Direction = FOP_WriteEntry;
	fop.pXAttr    = field;
	fop.lXAttr    = 64;
	fop.pFile     = (void *) pDir;
	fop.pFS       = (void *) this;

	bool DidTranslate = ProcessFOP( &fop );

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
	rstrncat( fname, pDir->Filename, MAX_PATH );

	zip_uint64_t zfi = zip_dir_add( za, (char *) fname, ZIP_FL_ENC_CP437 );

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

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		rstrncat( cpath, pDir->Filename, 255 );
		rstrncat( cpath, (BYTE *) "/", 255 );

		rstrncpy( pZDir->cpath, cpath, 255 );
	}

	return pDirectory->ReadDirectory();
}

int ZIPFile::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
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
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ZIPATTR;
	fop.Direction = FOP_WriteEntry;
	fop.pXAttr    = field;
	fop.lXAttr    = 64;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;

	bool DidTranslate = ProcessFOP( &fop );

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

int ZIPFile::Format_Process( DWORD FT, HWND hWnd )
{
	BYTE EmptyZIP[] = {
		'P', 'K', 0x05, 0x06,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00
	};

	pSource->WriteRaw( 0, sizeof( EmptyZIP ), EmptyZIP );

	pSource->Truncate( sizeof( EmptyZIP ) );

	PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

	return 0;
}

int ZIPFile::RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory )
{
	/* The idea here is to try and alter the name of the directory to something unique.

	   We're going to do this by copying the filename, then overwriting the trail with
	   a number starting at 1, and incrementing on each pass. The number is prefixed by _

	   When we find one, we'll return it. If not, we go around again. If we somehow fill
	   up all 10 characters, we'll error here.
	*/

	bool Done = false;

	BYTEString CurrentFilename( pDir->Filename, pDir->Filename.length() + 11 );
	BYTEString ProposedFilename( pDir->Filename.length() + 11 );

	size_t InitialLen = CurrentFilename.length();

	DWORD ProposedIndex = 1;

	BYTE ProposedSuffix[ 11 ];

	while ( !Done )
	{
		rstrncpy( ProposedFilename, CurrentFilename, InitialLen );

		rsprintf( ProposedSuffix, "_%d", ProposedIndex );

		WORD SuffixLen = rstrnlen( ProposedSuffix, 9 );
		
		/* If we allow long file names, then we just add it on the end, otherwise we have the 10 char limit */
		if ( ( InitialLen + SuffixLen ) <= 10 )
		{
			rstrcpy( &ProposedFilename[ InitialLen ], ProposedSuffix );
		}
		else
		{
			rstrncpy( &ProposedFilename[ 10 - SuffixLen ], ProposedSuffix, 10 );
		}

		/* Now see if that's unique */
		NativeFileIterator iFile;

		bool Unique = true;

		for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
		{
			if ( rstricmp( iFile->Filename, ProposedFilename ) )
			{
				Unique = false;

				break;
			}
		}

		Done = Unique;

		ProposedIndex++;

		if ( ProposedIndex == 0 )
		{
			/* EEP - Ran out */
			return NUTSError( 208, L"Unable to find unique name for incoming directory" );
		}
	}

	pDir->Filename = ProposedFilename;

	return 0;
}

std::vector<AttrDesc> ZIPFile::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	if ( pFile != nullptr )
	{
		FOPData fop;

		fop.DataType  = FOP_DATATYPE_ZIPATTR;
		fop.Direction = FOP_ExtraAttrs;
		fop.lXAttr    = 0;
		fop.pXAttr    = (BYTE *) &Attrs;
		fop.pFile     = (void *) pFile;
		fop.pFS       = nullptr;
	
		ProcessFOP( &fop );
	}

	return Attrs;
}
