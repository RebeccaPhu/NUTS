#include "StdAfx.h"
#include "ZIPCommon.h"

#include "libfuncs.h"
#include "SourceFunctions.h"

#include "zip.h"

const FSIdentifier FSID_ZIP = L"ZIP_File_FileSystem";

ZIPCommon::ZIPCommon(void)
{
	ReadPtr  = 0;
	WritePtr = 0;

	pWriteClone = nullptr;
}


ZIPCommon::~ZIPCommon(void)
{
	NixClone();
}

zip_uint64_t ZIPCommon::NUTSZipCallback( void *data, zip_uint64_t len, zip_source_cmd_t cmd )
{
	DataSource *pSource = GetSource();

	BYTE err[256];
	rsprintf( err, "%016X/%016X: %016X, %016X, %02X\n", this, pSource, data, len, cmd );
//	OutputDebugStringA( (char *) err );

	switch (cmd)
	{
	case ZIP_SOURCE_BEGIN_WRITE:
		NixClone();

		pWriteClone = new CTempFile();

		SourceToTemp( pSource, *pWriteClone );

		WritePtr = 0;

		return 0;

	case ZIP_SOURCE_BEGIN_WRITE_CLONING:
		WritePtr = len;

		NixClone();

		pWriteClone = new CTempFile();

		SourceToTemp( pSource, *pWriteClone );

		pWriteClone->SetExt( len );

		return 0;

	case ZIP_SOURCE_CLOSE:
		return 0;

	case ZIP_SOURCE_COMMIT_WRITE:
		ReplaceSourceContent( pSource, *pWriteClone );

		NixClone();

		return 0;

	case ZIP_SOURCE_ERROR:
		return 2 * sizeof(int);

	case ZIP_SOURCE_FREE:
		NixClone();

		return 0;

	case ZIP_SOURCE_OPEN:
		return 0;

	case ZIP_SOURCE_READ:
		if ( ReadPtr == pSource->PhysicalDiskSize )
		{
			return 0;
		}

		pSource->ReadRaw( ReadPtr, len, (BYTE *) data );

		ReadPtr += len;

		if ( ReadPtr > pSource->PhysicalDiskSize )
		{
			len -= ( ReadPtr - pSource->PhysicalDiskSize );

			ReadPtr = pSource->PhysicalDiskSize;
		}

		return len;

	case ZIP_SOURCE_REMOVE:
		return 0;

	case ZIP_SOURCE_ROLLBACK_WRITE:
		NixClone();

		return 0;

	case ZIP_SOURCE_SEEK:
		{
			zip_source_args_seek *args = ZIP_SOURCE_GET_ARGS( zip_source_args_seek, data, len, NULL );

			if ( args->whence == SEEK_SET )
			{
				ReadPtr = args->offset;
			}
			else if ( args->whence == SEEK_CUR )
			{
				ReadPtr += args->offset;
			}
			else
			{
				ReadPtr = pSource->PhysicalDiskSize + args->offset;
			}
		}

		return ReadPtr;

	case ZIP_SOURCE_SEEK_WRITE:
		{
			zip_source_args_seek *args = ZIP_SOURCE_GET_ARGS( zip_source_args_seek, data, len, NULL );

			if ( args->whence == SEEK_SET )
			{
				WritePtr = args->offset;
			}
			else if ( args->whence == SEEK_CUR )
			{
				WritePtr += args->offset;
			}
			else
			{
				WritePtr = pSource->PhysicalDiskSize + args->offset;
			}
		}

		return WritePtr;

	case ZIP_SOURCE_STAT:
		{
			zip_stat_t *zs = (zip_stat_t *) data;

			zip_stat_init( zs );

			/*
			#define ZIP_STAT_NAME 0x0001u
			#define ZIP_STAT_INDEX 0x0002u
			#define ZIP_STAT_SIZE 0x0004u
			#define ZIP_STAT_COMP_SIZE 0x0008u
			#define ZIP_STAT_MTIME 0x0010u
			#define ZIP_STAT_CRC 0x0020u
			#define ZIP_STAT_COMP_METHOD 0x0040u
			#define ZIP_STAT_ENCRYPTION_METHOD 0x0080u
			#define ZIP_STAT_FLAGS 0x0100u
			*/

			zs->size  = pSource->PhysicalDiskSize;
			zs->valid = ZIP_STAT_SIZE;
			zs->name  = NULL;
			zs->index = 0;
		}
		return 0;

	case ZIP_SOURCE_SUPPORTS:
		return zip_source_make_command_bitmap(
			ZIP_SOURCE_OPEN,
			ZIP_SOURCE_READ,
			ZIP_SOURCE_CLOSE,
			ZIP_SOURCE_STAT,
			ZIP_SOURCE_SEEK,
			ZIP_SOURCE_TELL,
			ZIP_SOURCE_SUPPORTS,
			ZIP_SOURCE_BEGIN_WRITE,
			ZIP_SOURCE_BEGIN_WRITE_CLONING,
			ZIP_SOURCE_COMMIT_WRITE,
			ZIP_SOURCE_ROLLBACK_WRITE,
			ZIP_SOURCE_TELL_WRITE,
			ZIP_SOURCE_SEEK_WRITE,
			ZIP_SOURCE_REMOVE,
			ZIP_SOURCE_WRITE,
			ZIP_SOURCE_ERROR
		);

	case ZIP_SOURCE_TELL:
		return ReadPtr;

	case ZIP_SOURCE_TELL_WRITE:
		return WritePtr;

	case ZIP_SOURCE_WRITE:
		pWriteClone->Seek( WritePtr );
		pWriteClone->Write( data, (DWORD) len );

		WritePtr += len;

		return len;
	}

	return -1;
}