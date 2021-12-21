#include "StdAfx.h"
#include "WindowsFileSystem.h"
#include "ImageDataSource.h"
#include "ZIPFile.h"
#include "NUTSError.h"

#include <direct.h>

WindowsFileSystem::WindowsFileSystem( std::wstring rootDrive ) : FileSystem(NULL) {
	pWindowsDirectory	= new WindowsDirectory(rootDrive);

	pWindowsDirectory->ReadDirectory();

	pDirectory = (Directory *) pWindowsDirectory;

	folderPath = rootDrive;

	FSID = FS_Windows;

	TopicIcon = FT_Windows;

	Flags = FSF_Uses_Extensions | FSF_Supports_Dirs | FSF_SupportFreeSpace | FSF_Capacity | FSF_Accepts_Sidecars;
}

WindowsFileSystem::~WindowsFileSystem(void) {
	delete pWindowsDirectory;
}

int	WindowsFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	if ( FileID > pDirectory->Files.size() )
	{
		return -1;
	}

	std::wstring destFile = folderPath;

	if ( destFile.substr( destFile.length() - 1 ) != L"\\" )
	{
		destFile += L"\\";
	}

	destFile += pWindowsDirectory->WindowsFiles[ FileID ];

	FILE	*f;
	
	_wfopen_s(&f, destFile.c_str(), L"rb");

	if ( f != nullptr )
	{
		fseek( f, 0, SEEK_END );
		DWORD Ext = ftell( f );
		fseek( f, 0, SEEK_SET );

		BYTE *Buffer = (BYTE *) malloc( 1048576 );

		while ( Ext > 0U )
		{
			DWORD rcount = 1048576;

			if ( Ext < rcount )
			{
				rcount = Ext;
			}

			fread( Buffer, 1, rcount, f );

			store.Write( Buffer, rcount );

			Ext -= rcount;
		}

		fclose(f);

		free( Buffer );
	}

	return FILEOP_SUCCESS;
}

int	WindowsFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( pFile->EncodingID != ENCODING_ASCII )
	{
		return FILEOP_NEEDS_ASCII;
	}

	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pFile, &*iFile ) )
		{
			if ( iFile->Flags & FF_Directory )
			{
				return FILEOP_ISDIR;
			}
			else
			{
				return FILEOP_EXISTS;
			}
		}
	}

	std::wstring destFile = folderPath;

	if ( destFile.substr( destFile.length() - 1 ) != L"\\" )
	{
		destFile += L"\\";
	}

	destFile += UString( (char *) pFile->Filename );

	if ( pFile->Flags & FF_Extension )
	{
		destFile += std::wstring( L"." ) + std::wstring( UString( (char *) pFile->Extension ) );
	}

	FILE	*f;
	
	_wfopen_s(&f, destFile.c_str(), L"wb");

	if ( f != nullptr )
	{
		QWORD Ext    = store.Ext();
		BYTE *Buffer = (BYTE *) malloc( 1048576 );

		store.Seek( 0 );

		while ( Ext > 0U )
		{
			DWORD wcount = 1048576;

			if ( Ext < wcount )
			{
				wcount = (DWORD) Ext;
			}

			store.Read( Buffer, wcount );

			fwrite( Buffer, 1, wcount, f );

			Ext -= wcount;
		}

		fclose(f);

		free( Buffer );
	}

	pDirectory->ReadDirectory();

	return FILEOP_SUCCESS;
}

int WindowsFileSystem::ChangeDirectory( DWORD FileID ) {
	if ( folderPath.substr( folderPath.length() - 1 ) != L"\\" )
	{
		folderPath += L"\\";
	}

	folderPath += pWindowsDirectory->WindowsFiles[FileID];

	delete pDirectory;

	pWindowsDirectory = new WindowsDirectory(folderPath);

	pWindowsDirectory->ReadDirectory();

	pDirectory = (Directory *) pWindowsDirectory;

	return 0;
}

int	WindowsFileSystem::Parent() {

	size_t pS = folderPath.find_last_of( L"\\" );

	if ( pS != std::wstring::npos )
	{
		folderPath = folderPath.substr( 0, pS );
	}

	if ( folderPath.length() < 3 )
	{
		folderPath += L"\\";
	}

	delete pWindowsDirectory;

	pWindowsDirectory	= new WindowsDirectory(folderPath);

	pWindowsDirectory->ReadDirectory();

	pDirectory = (Directory *) pWindowsDirectory;

	return 0;
}

int	WindowsFileSystem::CreateDirectory( NativeFile *pDir, DWORD CreateFlags ) {
	if ( pDir->EncodingID != ENCODING_ASCII )
	{
		return FILEOP_NEEDS_ASCII;
	}

	NativeFile SourceDir = *pDir;

	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pDir, &*iFile ) )
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
				/* No old map FSes use big directories, so all filenames are max. 10 chars */
				if ( RenameIncomingDirectory( &SourceDir, pDirectory ) != NUTS_SUCCESS )
				{
					return -1;
				}
			}
		}
	}

	std::wstring newPath = folderPath;

	if ( newPath.substr( newPath.length() - 1 ) != L"\\" )
	{
		newPath += L"\\";
	}

	newPath += UString( SourceDir.Filename );

	if ( SourceDir.Extension.length() > 0 )
	{
		newPath += L".";

		newPath += UString( SourceDir.Extension );
	}

	_wmkdir( newPath.c_str() );

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		delete pWindowsDirectory;

		pWindowsDirectory	= new WindowsDirectory(folderPath);

		pDirectory = (Directory *) pWindowsDirectory;
	}

	pWindowsDirectory->ReadDirectory();

	return 0;
}

int WindowsFileSystem::DeleteFile( DWORD FileID )
{
	std::wstring newPath = folderPath;

	if ( newPath.substr( newPath.length() - 1 ) != L"\\" )
	{
		newPath += L"\\";
	}

	newPath += pWindowsDirectory->WindowsFiles[ FileID ];

	if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
	{
		_wrmdir( newPath.c_str() );
	}
	else
	{
		_wunlink( newPath.c_str() );
	}

	pWindowsDirectory->ReadDirectory();

	return 0;
}

DataSource *WindowsFileSystem::FileDataSource( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		return nullptr;
	}

	std::wstring FilePath = folderPath;

	if ( FilePath.substr( folderPath.length() - 1 ) != L"\\" )
	{
		FilePath += L"\\";
	}

	FilePath += pWindowsDirectory->WindowsFiles[ FileID ];

	return new ImageDataSource( FilePath );
}

BYTE *WindowsFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE Title[ 512 ];

	std::wstring Path = folderPath;

	if ( pFile != nullptr ) 
	{
		if ( Path.substr( folderPath.length() - 1 ) != L"\\" )
		{
			Path += L"\\";
		}

		Path += pWindowsDirectory->WindowsFiles[ pFile->fileID ];

		if ( Flags & TF_Titlebar )
		{
			if ( !(Flags & TF_Final ) )
			{
				WCHAR chevron[4] = { L"   " };

				chevron[ 1 ] = 175;

				Path += std::wstring( chevron );
			}
		}
	}

	BYTE *pPath = (BYTE *) AString( (WCHAR *) Path.c_str() );

	strncpy_s( (char *) Title, 512, (char *) pPath, 511 );

	return Title;
}

int WindowsFileSystem::RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory )
{
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
		
		rstrcpy( &ProposedFilename[ InitialLen ], ProposedSuffix );

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

std::vector<AttrDesc> WindowsFileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Read-Only */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Read Only";
	Attrs.push_back( Attr );

	/* Archive */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Archive";
	Attrs.push_back( Attr );

	/* System */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"System";
	Attrs.push_back( Attr );

	/* Hidden */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Hidden";
	Attrs.push_back( Attr );

	/* Creation Time */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Creation Time";
	Attrs.push_back( Attr );

	/* Last Access Time */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Last Access Time";
	Attrs.push_back( Attr );

	/* Last Write Time */
	Attr.Index = 6;
	Attr.Type  = AttrVisible | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Last Write Time";
	Attrs.push_back( Attr );

	return Attrs;
}

WCHAR *WindowsFileSystem::Identify( DWORD FileID )
{
	// Use the base class first.
	WCHAR *pOrig = FileSystem::Identify( FileID );

	WCHAR *pIdent = pOrig;

	static WCHAR *DiskImage = L"Disk Image";
	static WCHAR *TapeImage = L"Casette Image";
	static WCHAR *HardImage = L"Hard Drive Image";
	static WCHAR *CDImage   = L"CD/DVD Image";

	// See if we can enhance this.
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pFile->Icon == FT_DiskImage )
	{
		pIdent = DiskImage;
	}
	else if ( pFile->Icon == FT_TapeImage )
	{
		pIdent = TapeImage;
	}
	else if ( pFile->Icon == FT_HardImage )
	{
		pIdent = HardImage;
	}
	else if ( pFile->Icon == FT_CDImage )
	{
		pIdent = CDImage;
	}

	return pIdent;
}

BYTE *WindowsFileSystem::DescribeFile( DWORD FileIndex )
{
	BYTE desc[1024];

	NativeFile *pFile = &pDirectory->Files[ FileIndex ];

	if ( pFile->Flags & FF_Extension )
	{
		rstrncpy( desc, pFile->Extension, pFile->Extension.length() );

		rsprintf( &desc[ pFile->Extension.length() ], " File, %s bytes", DisplayNumber( pFile->Length ) );
	}
	else
	{
		rsprintf( desc, "File, %s bytes", DisplayNumber( pFile->Length ) );
	}

	return desc;
}

BYTE *WindowsFileSystem::DisplayNumber( QWORD val )
{
	static BYTE NumString[ 64 ];
	static BYTE OutString[ 64 ];

	UINT64 v;
	int    s;

	rsprintf( NumString, "%llu", val );

	GetNumberFormatA(LOCALE_USER_DEFAULT, 0, (char *) NumString, NULL, (char *) OutString, 64 );  

	/* GetNumberFormat adds .00 to the end of this. Getting to not do so involves
	   arsing about with 400 GetLocaleInfo calls. Sod that. It's easier to just
	   remove the last 3 digits */

	WORD l = rstrnlen( OutString, 64 );

	if ( l >= 3 )
	{
		OutString[ l - 3 ] = 0;
	}

	return OutString;
}

int WindowsFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	ULARGE_INTEGER CapacityBytes;
	ULARGE_INTEGER FreeBytes;
	ULARGE_INTEGER CallerBytes; // We don't use this

	GetDiskFreeSpaceEx( folderPath.c_str(), &CallerBytes, &CapacityBytes, &FreeBytes );

	static FSSpace Map;

	Map.Capacity  = CapacityBytes.QuadPart;
	Map.pBlockMap = nullptr;
	Map.UsedBytes = CapacityBytes.QuadPart - FreeBytes.QuadPart;

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}

int WindowsFileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt )
{
	std::wstring WinName = UString( (char *) NewName );
	
	if ( rstrnlen( NewExt, 3 ) > 0 )
	{
		WinName += L"." + std::wstring( UString( (char *) NewExt ) );
	}

	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		bool Same = false;

		if ( ( rstricmp( iFile->Filename, NewName ) ) && ( FileID != iFile->fileID ) )
		{
			if ( iFile->Flags & FF_Extension )
			{
				if (  ( rstrcmp( iFile->Extension, NewExt ) ) && ( FileID != iFile->fileID ) )
				{
					Same = true;
				}
			}
			else
			{
				Same = true;
			}
		}

		if ( WinName == pWindowsDirectory->WindowsFiles[ iFile->fileID ] )
		{
			Same = true;
		}

		if ( Same )
		{
			return NUTSError( 0x206, L"An object with that name already exists" );
		}
	}

	if ( _wrename( pWindowsDirectory->WindowsFiles[ FileID ].c_str(), WinName.c_str() ) )
	{
		return NUTSError( 0x207, L"Windows failure renaming file" );
	}

	return NUTS_SUCCESS;
}