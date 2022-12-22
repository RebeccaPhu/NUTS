#include "StdAfx.h"
#include "ROMDiskDirectory.h"
#include "FOP.h"
#include "malloc.h"

void ROMDiskDirectory::CleanFiles()
{
	for ( NativeFileIterator iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		if ( ( iFile->pAuxData != nullptr ) && ( iFile->lAuxData > 0 ) )
		{
			free( (void *) iFile->pAuxData );
		}
	}

	for ( ResolvedIconList::iterator iIcon = ResolvedIcons.begin(); iIcon != ResolvedIcons.end(); iIcon++ )
	{
		if ( iIcon->second.pImage != nullptr )
		{
			free( (void *) iIcon->second.pImage );
		}
	}

	Files.clear();

	ResolvedIcons.clear();

	FileFOPData.clear();
}

int ROMDiskDirectory::ReadDirectory(void)
{
	CleanFiles();

	std::wstring DirectoryFile = ProgramDataPath + std::wstring( L"\\Directory-" ) + std::to_wstring( DirectoryID );

	CTempFile DirectoryIndex( DirectoryFile );

	DirectoryIndex.Keep(); // of course

	DirectoryIndex.Seek( 0 );

	QWORD Ext = DirectoryIndex.Ext();

	if ( Ext > 0 )
	{
		DWORD p = 0;

		DWORD FileID = 0;

		while ( ( Ext - p ) >= sizeof( DWORD ) )
		{
			DWORD FileIndex = 0;

			DirectoryIndex.Read( &FileIndex, sizeof( DWORD ) );

			NativeFile file;

			ReadNativeFile( FileIndex, &file );

			file.fileID = FileID;


			p += sizeof( DWORD );

			FileID++;

			Files.push_back( file );

			FOPData fop;

			fop.DataType  = FOP_DATATYPE_ROMDISK;
			fop.Direction = FOP_ReadEntry;
			fop.lXAttr    = 0;
			fop.pXAttr    = nullptr;
			fop.pFS       = (void *) pSrcFS;
			fop.pFile     = (void *) &Files.back();

			(void) ProcessFOP( &fop );

			if ( fop.ReturnData.StatusString[ 0 ] != 0 )
			{
				FileFOPData[ file.fileID ] = fop.ReturnData;
			}
		}
	}

	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ROMDISK;
	fop.Direction = FOP_PostRead;
	fop.lXAttr    = 0;
	fop.pXAttr    = nullptr;
	fop.pFile     = nullptr;
	fop.pFS       = pSrcFS;

	(void) ProcessFOP( &fop );
	
	return NUTS_SUCCESS;
}

int ROMDiskDirectory::WriteDirectory(void)
{
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ROMDISK;
	fop.Direction = FOP_PreWrite;
	fop.lXAttr    = 0;
	fop.pXAttr    = nullptr;
	fop.pFile     = nullptr;
	fop.pFS       = pSrcFS;

	(void) ProcessFOP( &fop );

	std::wstring DirectoryFile = ProgramDataPath + std::wstring( L"\\Directory-" ) + std::to_wstring( DirectoryID );

	CTempFile DirectoryIndex;

	DirectoryIndex.Seek( 0 );

	for ( NativeFileIterator iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		DWORD FileIndex = iFile->Attributes[ 16U ];

		DirectoryIndex.Write( (void *) &FileIndex, sizeof( DWORD ) );

		WriteNativeFile( FileIndex, &*iFile ); // Don't ask.
	}

	_wunlink( DirectoryFile.c_str() );

	DirectoryIndex.KeepAs( DirectoryFile );

	return NUTS_SUCCESS;
}

void ROMDiskDirectory::ReadNativeFile( QWORD FileIndex, NativeFile *pFile )
{
	std::wstring NativeFileFile = ProgramDataPath + L"\\NativeFile-" + std::to_wstring( FileIndex );

	CTempFile NativeFileData( NativeFileFile );

	NativeFileData.Keep();

	NativeFileData.Seek( 0 );

	NativeFileData.Read( &pFile->Flags, sizeof( DWORD ) );

	pFile->Filename = ReadString( NativeFileData );
	
	if ( pFile->Flags & FF_Extension )
	{
		pFile->Extension = ReadString( NativeFileData );
	}

	NativeFileData.Read( &pFile->Length, sizeof( QWORD ) );

	DWORD Data;

	NativeFileData.Read( &Data, sizeof( DWORD ) ); pFile->Type       = (FileType) Data;
	NativeFileData.Read( &Data, sizeof( DWORD ) ); pFile->Icon       = (DWORD)    Data;
	NativeFileData.Read( &Data, sizeof( DWORD ) ); pFile->ExtraForks = (BYTE)     Data;
	NativeFileData.Read( &Data, sizeof( DWORD ) ); pFile->lAuxData   = (DWORD)    Data;

	if ( pFile->lAuxData > 0U )
	{
		pFile->pAuxData = (BYTE *) malloc( pFile->lAuxData );

		NativeFileData.Read( (void *) pFile->pAuxData, pFile->lAuxData );
	}

	for ( DWORD ai=0; ai<16U; ai++ )
	{
		NativeFileData.Read( &pFile->Attributes[ ai ], sizeof( DWORD ) );
	}

	pFile->Attributes[ 16U ] = (DWORD) FileIndex;

	pFile->FSFileTypeX = ROMDiskFSType;

	pFile->FSFileType  = (FTIdentifier)       UString( (char *) (BYTE *) ReadString( NativeFileData ) );
	pFile->EncodingID  = (EncodingIdentifier) UString( (char *) (BYTE *) ReadString( NativeFileData ) );
	pFile->XlatorID    = (TXIdentifier)       UString( (char *) (BYTE *) ReadString( NativeFileData ) );

	if ( pFile->EncodingID == L"" )
	{
		pFile->EncodingID = ENCODING_ASCII;
	}
}

BYTEString ROMDiskDirectory::ReadString( CTempFile &file )
{
	DWORD DataLen = 0;

	file.Read( &DataLen, sizeof( DWORD ) );

	AutoBuffer StringRead( DataLen );

	file.Read( (void *) (BYTE *) StringRead, DataLen );

	return BYTEString( (BYTE *) StringRead, DataLen );
}

void ROMDiskDirectory::WriteNativeFile( QWORD FileIndex, NativeFile *pFile )
{
	std::wstring NativeFileFile = ProgramDataPath + L"\\NativeFile-" + std::to_wstring( FileIndex );

	CTempFile NativeFileData;

	NativeFileData.Seek( 0 );

	NativeFileData.Write( &pFile->Flags, sizeof( DWORD ) );

	WriteString( NativeFileData, pFile->Filename );
	
	if ( pFile->Flags & FF_Extension )
	{
		WriteString( NativeFileData, pFile->Extension );
	}

	NativeFileData.Write( &pFile->Length, sizeof( QWORD ) );

	DWORD Data;

	Data = (DWORD) pFile->Type;       NativeFileData.Write( &Data, sizeof( DWORD ) );
	Data = (DWORD) pFile->Icon;       NativeFileData.Write( &Data, sizeof( DWORD ) );
	Data = (DWORD) pFile->ExtraForks; NativeFileData.Write( &Data, sizeof( DWORD ) );
	Data = (DWORD) pFile->lAuxData;   NativeFileData.Write( &Data, sizeof( DWORD ) );

	if ( pFile->lAuxData > 0U )
	{
		NativeFileData.Write( (void *) pFile->pAuxData, pFile->lAuxData );
	}

	for ( DWORD ai=0; ai<16U; ai++ )
	{
		NativeFileData.Write( &pFile->Attributes[ ai ], sizeof( DWORD ) );
	}

	WriteString( NativeFileData, BYTEString( (BYTE *) AString( (WCHAR *) pFile->FSFileType.c_str() ) ) );
	WriteString( NativeFileData, BYTEString( (BYTE *) AString( (WCHAR *) pFile->EncodingID.c_str() ) ) );
	WriteString( NativeFileData, BYTEString( (BYTE *) AString( (WCHAR *) pFile->XlatorID.c_str() ) ) );

	_wunlink( NativeFileFile.c_str() );

	NativeFileData.KeepAs( NativeFileFile );
}

void ROMDiskDirectory::WriteString( CTempFile &file, BYTEString &strng )
{
	DWORD DataLen = (DWORD) strng.length();

	file.Write( &DataLen, sizeof( DWORD ) );
	file.Write( (void *) (BYTE *) strng, DataLen );
}

