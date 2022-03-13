#include "stdafx.h"

#include "ZIPDirectory.h"
#include "NUTSError.h"
#include "Defs.h"
#include "Plugins.h"
#include "ExtensionRegistry.h"

#include "libfuncs.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "zip.h"
#include "archive.h"
#include "archive_entry.h"
#include "ZipFuncs.h"

int ZIPDirectory::ReadDirectory(void)
{
	Files.clear();

	zip_error_t ze;

	zip_error_init( &ze );

	int r;

	DWORD FileID = 0;
	DWORD SeqID  = 0;

	zip_source_t *z = zip_source_function_create( (zip_source_callback) _NUTSZipCallback, (ZIPCommon *) this, &ze );

	zip_t *za = zip_open_from_source( z, 0, &ze );

	if ( za == NULL )
	{
		return NUTSError( 0xA0, UString( (char *) zip_error_strerror( &ze ) ) );
	}

	DWORD NumEntries = zip_get_num_entries( za, NULL );

	WORD l = rstrnlen( cpath, 255 );

	while ( SeqID < NumEntries )
	{
		BYTE *fname = (BYTE *) zip_get_name( za, SeqID, NULL );

		BYTE *pDir = ZIPSubPath( cpath, fname );

		if ( pDir != nullptr )
		{
			NativeFileIterator iFile;

			bool HaveThatDir = false;

			for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
			{
				if ( rstrnicmp( iFile->Filename, pDir, 255 ) )
				{
					HaveThatDir = true;
				}
			}
				
			WORD fl = rstrnlen( fname, 255 ) - 1;

			if ( ( fname[ fl ] == '/' ) && ( !HaveThatDir ) )
			{
				NativeFile file;

				file.EncodingID = ENCODING_ASCII;
				file.fileID     = FileID;
				file.FSFileType = FT_ZIP;
				file.Icon       = FT_Directory;
				file.Type       = FT_Directory;
				file.Flags      = FF_Directory;
				file.Length     = 0;

				file.Filename = BYTEString( pDir );

				file.Attributes[ 0 ] = SeqID;

				Files.push_back( file );

				FileID++;

				zip_flags_t attrSource = ZIP_FL_LOCAL;

				DWORD NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );

				if ( NumFields == 0xFFFFFFFF )
				{
					attrSource = ZIP_FL_CENTRAL;

					NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );
				}

				bool DidTranslate = false;

				if ( NumFields != 0xFFFFFFFF )
				{
					for ( DWORD f = 0; f < NumFields; f++ )
					{
						BYTE field[64];

						ZeroMemory( field, 64 );

						zip_uint16_t fl;

						BYTE *srcField = (BYTE *) zip_file_extra_field_get( za, SeqID, f, (zip_uint16_t *) field, &fl, attrSource );

						memcpy( &field[4], srcField, min( 60, fl ) );

						* (WORD *) &field[2] = fl;

						FOPData fop;

						fop.DataType  = FOP_DATATYPE_ZIPATTR;
						fop.Direction = FOP_ReadEntry;
						fop.pXAttr    = field;
						fop.lXAttr    = fl;
						fop.pFile     = (void *) &Files.back();
						fop.pFS       = (void *) srcFS;

						DidTranslate = ProcessFOP( &fop );
					}
				}
			}
		}

		if ( IsCPath( cpath, fname ) )
		{
			zip_stat_t zs;
			zip_stat_init( &zs );
			zip_stat_index( za, SeqID, NULL, &zs );

			NativeFile file;

			file.EncodingID = ENCODING_ASCII;
			file.fileID     = FileID;
			file.Flags      = 0;
			file.FSFileType = FT_ZIP;

			file.Icon       = FT_Arbitrary;
			file.Length     = zs.size;
			file.Type       = FT_Arbitrary;

			file.HasResolvedIcon= false;

			file.Filename = BYTEString( ZIPSubName( cpath, fname ) );

			file.Attributes[ 0 ] = SeqID;

			Files.push_back( file );

			FileID++;

			zip_flags_t attrSource = ZIP_FL_LOCAL;

			DWORD NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );

			if ( NumFields == 0xFFFFFFFF )
			{
				attrSource = ZIP_FL_CENTRAL;

				NumFields = zip_file_extra_fields_count( za, SeqID, attrSource );
			}

			bool DidTranslate = false;

			if ( ( NumFields != 0xFFFFFFFF ) && ( !CloneWars ) )
			{
				for ( DWORD f = 0; f < NumFields; f++ )
				{
					BYTE field[64];

					ZeroMemory( field, 64 );

					zip_uint16_t fl;

					BYTE *srcField = (BYTE *) zip_file_extra_field_get( za, SeqID, f, (zip_uint16_t *) field, &fl, attrSource );

					memcpy( &field[4], srcField, min( 60, fl ) );

					* (WORD *) &field[2] = fl;

					FOPData fop;

					fop.DataType  = FOP_DATATYPE_ZIPATTR;
					fop.Direction = FOP_ReadEntry;
					fop.pXAttr    = field;
					fop.lXAttr    = fl;
					fop.pFile     = (void *) &Files.back();
					fop.pFS       = srcFS;

					DidTranslate = ProcessFOP( &fop );
				}
			}

			if ( !DidTranslate )
			{
				TranslateFileType( &Files.back() );
			}

		}

		SeqID++;
	}

	zip_error_fini( &ze );
	zip_source_close( z );
	zip_source_free( z );

	return 0;
}

int ZIPDirectory::WriteDirectory(void)
{
	return 0;
}

void ZIPDirectory::TranslateFileType(NativeFile *file) {
	if ( file->Flags & FF_Directory )
	{
		file->Type = FT_Directory;
		file->Icon = FT_Directory;

		return;
	}

	BYTE *pDot = rstrrchr( file->Filename, '.', 255 );

	if ( ( pDot != nullptr ) && ( file->Filename[ 0 ] != (BYTE) '.' ) )
	{
		file->Extension = BYTEString( pDot + 1 );

		/* This looks weird, but is fine. We're going to zero off the dot,
		   create a new BYTEString from the BYTE * data pointed to by the
		   old, and the set the old to the new. */

		*pDot = 0;

		file->Filename = BYTEString( file->Filename );

		file->Flags |= FF_Extension;
	}

	if ( ! ( file->Flags & FF_Extension ) )
	{
		file->Type = FT_Arbitrary;
		file->Icon = FT_Arbitrary;

		return;
	}

	std::wstring ThisExt = std::wstring( (WCHAR *) UString( (char *) file->Extension ) );

	ExtDef Desc = ExtReg.GetTypes( ThisExt );

	file->Type = Desc.Type;
	file->Icon = Desc.Icon;

	if ( file->Type == FT_Text ) { file->XlatorID = TUID_TEXT; }
}
