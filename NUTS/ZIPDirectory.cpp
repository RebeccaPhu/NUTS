#include "stdafx.h"

#include "ZIPDirectory.h"
#include "NUTSError.h"
#include "Defs.h"
#include "Plugins.h"

#include "libfuncs.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "archive.h"
#include "archive_entry.h"
#include "ZipFuncs.h"

int ZIPDirectory::ReadDirectory(void)
{
	Files.clear();

	struct archive *a;
	struct archive_entry *entry;

	int r;

	DWORD FileID = 0;

	a = archive_read_new();

	archive_read_support_filter_all( a );
	archive_read_support_format_all( a );

	r = archive_read_open_filename (a, pSource->GetLocation(), 10240 );

	if ( r != ARCHIVE_OK )
	{
		return NUTSError( 0xA0, ZIPError( a ) );
	}

	while ( ( r = archive_read_next_header( a, &entry ) ) == ARCHIVE_OK )
	{
		NativeFile file;

		file.EncodingID = ENCODING_ASCII;
		file.fileID     = FileID;
		file.Flags      = 0;
		file.FSFileType = FT_ZIP;
		file.Icon       = FT_Arbitrary;
		file.Length     = archive_entry_size( entry );
		file.Type       = FT_Arbitrary;
		file.XlatorID   = 0;

		file.HasResolvedIcon= false;

		rstrncpy( file.Filename, (BYTE *) archive_entry_pathname( entry ), 255 );

		BYTE *pExtra = (BYTE *) archive_entry_extra_data( entry );

		FSPlugins.TranslateZIPContent( &file, pExtra );

		Files.push_back( file );

		FileID++;

		archive_read_data_skip( a );
	}

	r = archive_read_free( a );

	if ( r != ARCHIVE_OK )
	{
		return NUTSError( 0xA0, ZIPError( a ) );
	}

	return 0;
}

int ZIPDirectory::WriteDirectory(void)
{
	return 0;
}

