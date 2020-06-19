#include "StdAfx.h"
#include "WindowsFileSystem.h"
#include "ImageDataSource.h"
#include "ZIPFile.h"

WindowsFileSystem::WindowsFileSystem( std::wstring rootDrive ) : FileSystem(NULL) {
	pWindowsDirectory	= new WindowsDirectory(rootDrive);

	pWindowsDirectory->ReadDirectory();

	pDirectory = (Directory *) pWindowsDirectory;

	folderPath = rootDrive;

	FSID = FS_Windows;
}

WindowsFileSystem::~WindowsFileSystem(void) {
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

int	WindowsFileSystem::CreateDirectory(BYTE *Filename, bool EnterAfter) {

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

BYTE *WindowsFileSystem::GetTitleString( NativeFile *pFile )
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
		
		WCHAR chevron[4] = { L"   " };

		chevron[ 1 ] = 175;

		Path += std::wstring( chevron );
	}

	BYTE *pPath = (BYTE *) AString( (WCHAR *) Path.c_str() );

	strncpy_s( (char *) Title, 512, (char *) pPath, 511 );

	return Title;
}