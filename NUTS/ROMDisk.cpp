#include "StdAfx.h"
#include "ROMDisk.h"

// WTF MICROSOFT?!?! DIRECT.H !?!?!?!
#include <direct.h>
#include <errno.h>

CROMDiskLock ROMDiskLock;

int ROMDisk::Init(void)
{
	if ( ProgramDataPath == L"" )
	{
		return NUTSError( 0x10B, L"Could not obtain ROMDisk store path" );
	}

	if ( _wmkdir( ProgramDataPath.c_str() ) != 0 )
	{
		if ( errno != EEXIST )
		{
			return NUTSError( 0x10A, L"Could not create ROMDisk store. Check permissions on C:\\ProgramData" );
		}
	}

	pROMDiskDir->ProcessFOP = ProcessFOP;

	return pDirectory->ReadDirectory();
}

int ROMDisk::WriteFile(NativeFile *pFile, CTempFile &store)
{
	DWORD NextIndex = ROMDiskLock.GetNextFileIndex();

	// Add it to the directory
	pDirectory->Files.push_back( *pFile );

	pDirectory->Files.back().Attributes[ 16U ] = NextIndex;

	// Duplicate aux data
	if ( pFile->lAuxData > 0 )
	{
		pDirectory->Files.back().pAuxData = (BYTE *) malloc( pFile->lAuxData );

		(void) memcpy(
			(void *) pDirectory->Files.back().pAuxData,
			(const void *) pFile->pAuxData,
			(size_t) pFile->lAuxData
			);
	}

	// Write the file data.
	CTempFile FileData;

	FileData.Seek( 0 );
	store.Seek( 0 );

	QWORD BytesToGo = store.Ext();

	BYTE Buf[ 32768 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesCopy = min( (DWORD) BytesToGo, 32768 );

		store.Read( (void *) Buf, BytesCopy );

		FileData.Write( (void *) Buf, BytesCopy );

		BytesToGo -= BytesCopy;
	}

	std::wstring FileDataName = ProgramDataPath + std::wstring( L"\\FileData-" ) + std::to_wstring( (QWORD) NextIndex );

	FileData.KeepAs( FileDataName );

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	// Write any forks
	for ( BYTE ForkID=0; ForkID<pFile->ExtraForks; ForkID++ )
	{
		std::wstring FileForkName = ProgramDataPath + std::wstring( L"\\FileFork-" ) + std::to_wstring( (QWORD) NextIndex ) + L"_" + std::to_wstring( (QWORD) ForkID );

		CTempFile FileFork;

		FileFork.Seek( 0 );

		Forks[ ForkID ]->Seek( 0 );

		QWORD BytesToGo = Forks[ ForkID ]->Ext();

		BYTE Buf[ 32768 ];

		while ( BytesToGo > 0 )
		{
			DWORD BytesCopy = min( (DWORD) BytesToGo, 32768 );

			Forks[ ForkID ]->Read( (void *) Buf, BytesCopy );

			FileFork.Write( (void *) Buf, BytesCopy );

			BytesToGo -= BytesCopy;
		}

		FileFork.KeepAs( FileForkName );
	}

	return pDirectory->ReadDirectory();
}

int ROMDisk::ReadFile(DWORD FileID, CTempFile &store)
{
	std::wstring FileDataName = ProgramDataPath + std::wstring( L"\\FileData-" ) + std::to_wstring( (QWORD) pDirectory->Files[ FileID ].Attributes[ 16U ] );

	CTempFile FileData( FileDataName );

	FileData.Keep();

	QWORD BytesToGo = pDirectory->Files[ FileID ].Length;

	BYTE Buf[ 32768 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesCopy = min( (DWORD) BytesToGo, 32768 );

		FileData.Read( (void *) Buf, BytesCopy );

		store.Write( (void *) Buf, BytesCopy );

		BytesToGo -= BytesCopy;
	}

	return NUTS_SUCCESS;
}

int ROMDisk::ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj )
{
	std::wstring FileForkName = ProgramDataPath + std::wstring( L"\\FileFork-" ) + std::to_wstring( (QWORD) pDirectory->Files[ FileID ].Attributes[ 16U ] ) + L"_" + std::to_wstring( (QWORD) ForkID );

	CTempFile FileFork( FileForkName );

	FileFork.Keep();

	QWORD BytesToGo = FileFork.Ext();

	BYTE Buf[ 32768 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesCopy = min( (DWORD) BytesToGo, 32768 );

		FileFork.Read( (void *) Buf, BytesCopy );

		forkObj.Write( (void *) Buf, BytesCopy );

		BytesToGo -= BytesCopy;
	}

	return NUTS_SUCCESS;
}

BYTE *ROMDisk::GetTitleString( NativeFile *pFile, DWORD Flags ) {
	static BYTE ISOPath[512];

	rstrncpy( ISOPath, (BYTE *) "ROMDISK:", 511 );

	rstrncat( ISOPath, (BYTE *) AString( (WCHAR *) VirtualPath.c_str() ), 511 );

	if ( pFile != nullptr )
	{
		rstrncat( ISOPath, (BYTE *) "/", 511 );
		rstrncat( ISOPath, (BYTE *) pFile->Filename, 511 );

		if ( pFile->Flags & FF_Extension )
		{
			rstrncat( ISOPath, (BYTE *) ".", 511 );
			rstrncat( ISOPath, (BYTE *) pFile->Extension, 511 );
		}

		if ( Flags & TF_Titlebar )
		{
			if ( !(Flags & TF_Final ) )
			{
				static BYTE chevron[4] = { 0x20, 0xAF, 0x20, 0x00 };

				rstrncat( ISOPath, chevron, 511 );
			}
		}
	}

	return ISOPath;
}

BYTE *ROMDisk::DescribeFile( DWORD FileIndex )
{
	static BYTE status[ 128 ];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	if ( pROMDiskDir->FileFOPData.find( FileIndex ) != pROMDiskDir->FileFOPData.end() )
	{
		rstrncpy( status, pROMDiskDir->FileFOPData[ FileIndex ].Descriptor, 128 );
	}
	else if ( pFile->Flags & FF_Directory )
	{
		rsprintf( status, "Directory" );
	}
	else if ( pFile->ExtraForks > 0 )
	{
		std::wstring FileForkName = ProgramDataPath + std::wstring( L"\\FileFork-" ) + std::to_wstring( (QWORD) pDirectory->Files[ FileIndex ].Attributes[ 16U ] ) + L"_0";

		CTempFile FileFork( FileForkName );

		FileFork.Keep();

		QWORD ForkSize = FileFork.Ext();

		rsprintf( status, "File, %d/%d bytes", pFile->Length, ForkSize );
	}
	else
	{
		rsprintf( status, "File, %d bytes", pFile->Length );
	}
		
	return status;
}

BYTE *ROMDisk::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[128];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else if ( pROMDiskDir->FileFOPData.find( FileIndex ) != pROMDiskDir->FileFOPData.end() )
	{
		rstrncpy( status, pROMDiskDir->FileFOPData[ FileIndex ].StatusString, 128 );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s, Directory", (BYTE *) pFile->Filename	);
		}
		else
		{
			if ( pFile->Flags & FF_Extension )
			{
				rsprintf( status, "%s.%s, %d bytes",
					(BYTE *) pFile->Filename, (BYTE *) pFile->Extension, (DWORD) pFile->Length
				);
			}
			else
			{
				rsprintf( status, "%s, %d bytes",
					(BYTE *) pFile->Filename, (DWORD) pFile->Length
				);
			}

			if ( pFile->ExtraForks > 0 )
			{
				std::wstring FileForkName = ProgramDataPath + std::wstring( L"\\FileFork-" ) + std::to_wstring( (QWORD) pDirectory->Files[ FileIndex ].Attributes[ 16U ] ) + L"_0";

				CTempFile FileFork( FileForkName );

				FileFork.Keep();

				QWORD ForkSize = FileFork.Ext();

				rsprintf( &status[ rstrnlen( status, 128 ) ], " %d bytes in fork", ForkSize );
			}
		}
	}
		
	return status;
}

WCHAR *ROMDisk::Identify( DWORD FileID )
{
	if ( pROMDiskDir->FileFOPData.find( FileID ) != pROMDiskDir->FileFOPData.end() )
	{
		return (WCHAR *) pROMDiskDir->FileFOPData[ FileID ].Identifier.c_str();
	}

	return FileSystem::Identify( FileID );
}


int ROMDisk::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	ULARGE_INTEGER CapacityBytes;
	ULARGE_INTEGER FreeBytes;
	ULARGE_INTEGER CallerBytes; // We don't use this

	GetDiskFreeSpaceEx( ProgramDataPath.c_str(), &CallerBytes, &CapacityBytes, &FreeBytes );

	static FSSpace Map;

	Map.Capacity  = CapacityBytes.QuadPart;
	Map.pBlockMap = nullptr;
	Map.UsedBytes = CapacityBytes.QuadPart - FreeBytes.QuadPart;

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}


int	ROMDisk::CreateDirectory( NativeFile *pDir, DWORD CreateFlags )
{
	NativeFile SourceDir = *pDir;

	std::wstring newPath = VirtualPath;

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

	DWORD NextIndex = ROMDiskLock.GetNextFileIndex();

	// Add it to the directory
	pDirectory->Files.push_back( *pDir );

	pDirectory->Files.back().Attributes[ 16U ] = NextIndex;

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		pROMDiskDir->DirectoryID = NextIndex;

		VirtualPath = newPath;

		PathStack.push_back( std::make_pair<DWORD, std::wstring>( NextIndex, VirtualPath ) );
	}

	return pDirectory->ReadDirectory();
}

int ROMDisk::ChangeDirectory( DWORD FileID )
{
	DWORD NextIndex = pDirectory->Files[ FileID ].Attributes[ 16U ];

	std::wstring newPath = VirtualPath;

	if ( newPath.substr( newPath.length() - 1 ) != L"/" )
	{
		newPath += L"/";
	}

	newPath += UString( pDirectory->Files[ FileID ].Filename );

	if ( pDirectory->Files[ FileID ].Extension.length() > 0 )
	{
		newPath += L".";

		newPath += UString( pDirectory->Files[ FileID ].Extension );
	}

	pROMDiskDir->DirectoryID = NextIndex;

	VirtualPath = newPath;

	PathStack.push_back( std::make_pair<DWORD, std::wstring>( NextIndex, VirtualPath ) );

	return pDirectory->ReadDirectory();
}

int ROMDisk::Parent()
{
	PathStack.pop_back();

	VirtualPath = PathStack.back().second;

	pROMDiskDir->DirectoryID = PathStack.back().first;

	return pDirectory->ReadDirectory();
}


std::vector<AttrDesc> ROMDisk::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	if ( pFile != nullptr )
	{
		FOPData fop;

		fop.DataType  = FOP_DATATYPE_CDISO;
		fop.Direction = FOP_ExtraAttrs;
		fop.lXAttr    = 0;
		fop.pXAttr    = (BYTE *) &Attrs;
		fop.pFile     = (void *) pFile;
		fop.pFS       = nullptr;
	
		ProcessFOP( &fop );
	}

	AttrDesc Attr;

	/* Virtual Index. Hex, visible, disabled */
	Attr.Index = 16;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Virtual Index";
	Attrs.push_back( Attr );

	return Attrs;
}


int ROMDisk::SetProps( DWORD FileID, NativeFile *Changes )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	for ( BYTE i=16; i<32; i++ )
	{
		pFile->Attributes[ i ] = Changes->Attributes[ i ];
	}
	
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_ROMDISK;
	fop.Direction = FOP_AttrChanges;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;
	fop.pXAttr    = (BYTE *) Changes;

	(void) ProcessFOP( &fop );

	fop.DataType  = FOP_DATATYPE_ROMDISK;
	fop.Direction = FOP_WriteEntry;
	fop.lXAttr    = 0;
	fop.pXAttr    = nullptr;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;

	(void) ProcessFOP( &fop );

	// Since this is all in the NativeFile, we can just call WriteDirectory;

	return pDirectory->WriteDirectory();
}

FileSystem *ROMDisk::FileFilesystem( DWORD FileID )
{
	if ( pROMDiskDir->FileFOPData.find( FileID ) != pROMDiskDir->FileFOPData.end() )
	{
		if ( pROMDiskDir->FileFOPData[ FileID ].ProposedFS != L"" )
		{
			DataSource *pSource = FileDataSource( FileID );

			if ( pSource != nullptr )
			{
				FileSystem *pNewFS = (FileSystem *) LoadFOPFS( pROMDiskDir->FileFOPData[ FileID ].ProposedFS, (void *) pSource );

				DS_RELEASE( pSource );

				return pNewFS;
			}

			DS_RELEASE( pSource );
		}
	}

	return nullptr;
}

int ROMDisk::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt )
{
	pDirectory->Files[ FileID ].Filename = NewName;

	if ( pDirectory->Files[ FileID ].Flags & FF_Extension )
	{
		pDirectory->Files[ FileID ].Extension = NewExt;
	}

	return pDirectory->WriteDirectory();
}

int ROMDisk::DeleteFile( DWORD FileID )
{
	DWORD VirtualIndex = pDirectory->Files[ FileID ].Attributes[ 16U ];

	std::wstring NativeFileName = ProgramDataPath + std::wstring( L"\\NativeFile-" ) + std::to_wstring( (QWORD) VirtualIndex );

	if ( ( _wunlink( NativeFileName.c_str() ) != 0 ) && ( errno != ENOENT ) )
	{
		return NUTSError( 0x10C, L"Error deleting file descriptor" );
	}

	std::wstring FileDataName = ProgramDataPath + std::wstring( L"\\FileData-" ) + std::to_wstring( (QWORD) VirtualIndex );

	if ( ( _wunlink( FileDataName.c_str() ) != 0 ) && ( errno != ENOENT ) )
	{
		return NUTSError( 0x10C, L"Error deleting file data" );
	}

	std::wstring AuxDataName = ProgramDataPath + std::wstring( L"\\AuxData-" ) + std::to_wstring( (QWORD) VirtualIndex );

	if ( ( _wunlink( AuxDataName.c_str() ) != 0 ) && ( errno != ENOENT ) )
	{
		return NUTSError( 0x10C, L"Error deleting aux data" );
	}

	for ( BYTE ForkID = 0; ForkID < pDirectory->Files[ FileID ].ExtraForks; ForkID++ )
	{
		std::wstring FileForkName = ProgramDataPath + std::wstring( L"\\FileFork-" ) + std::to_wstring( (QWORD) VirtualIndex ) + L"_" + std::to_wstring( (QWORD) ForkID );

		if ( ( _wunlink( FileForkName.c_str() ) != 0 ) && ( errno != ENOENT ) )
		{
			return NUTSError( 0x10C, L"Error deleting file fork" );
		}
	}

	if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
	{
		std::wstring NativeFileName = ProgramDataPath + std::wstring( L"\\Directory-" ) + std::to_wstring( (QWORD) VirtualIndex );

		if ( ( _wunlink( NativeFileName.c_str() ) != 0 ) && ( errno != ENOENT ) )
		{
			return NUTSError( 0x10C, L"Error deleting directory index" );
		}
	}

	(void) pDirectory->Files.erase( pDirectory->Files.begin() + FileID );

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

LocalCommands ROMDisk::GetLocalCommands( void )
{
	LocalCommand c1 = { LC_Always, L"Empty ROM Disk" }; 
	LocalCommands cmds;

	cmds.HasCommandSet = true;
	cmds.Root          = L"ROM Disk";

	cmds.CommandSet.push_back( c1 );

	return cmds;
}

int ROMDisk::ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
{
	if ( MessageBox( hParentWnd,
		L"This will complete empty the contents of the ROM Disk.\n\n"
		L"This operation cannot be undone. Do you want to continue?",
		L"NUTS ROM Disk", MB_ICONWARNING | MB_YESNO ) == IDNO )
	{
		return NUTS_SUCCESS;
	}

	WIN32_FIND_DATA d;

	HANDLE hFind = FindFirstFile( std::wstring( ProgramDataPath + L"\\*" ).c_str(), &d );

	while ( hFind != INVALID_HANDLE_VALUE )
	{
		if ( ! ( d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			if ( wcsncmp( d.cFileName, L"Directory-", 10 ) == 0 )
			{
				_wunlink( std::wstring( ProgramDataPath + L"\\" + d.cFileName ).c_str() );
			}

			if ( wcsncmp( d.cFileName, L"FileFork-", 9 ) == 0 )
			{
				_wunlink( std::wstring( ProgramDataPath + L"\\" + d.cFileName ).c_str() );
			}

			if ( wcsncmp( d.cFileName, L"FileData-", 9 ) == 0 )
			{
				_wunlink( std::wstring( ProgramDataPath + L"\\" + d.cFileName ).c_str() );
			}

			if ( wcsncmp( d.cFileName, L"NativeFile-", 11 ) == 0 )
			{
				_wunlink( std::wstring( ProgramDataPath + L"\\" + d.cFileName ).c_str() );
			}
		}

		if ( !FindNextFile( hFind, &d ) )
		{
			FindClose( hFind );

			hFind = INVALID_HANDLE_VALUE;
		}
	}

	pROMDiskDir->Empty();
	
	VirtualPath = L"/";

	PathStack.clear();

	if ( pDirectory->ReadDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	return CMDOP_REFRESH;
}

int ROMDisk::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	std::wstring FileDataName = ProgramDataPath + std::wstring( L"\\FileData-" ) + std::to_wstring( (QWORD) pFile->Attributes[ 16U ] );

	CTempFile newStore;

	QWORD BytesToGo = store.Ext();

	BYTE Buf[ 32768 ];

	store.Seek( 0 );
	newStore.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesLeft = min( (DWORD) BytesToGo, 32768 );

		store.Read( (void *) Buf, BytesLeft );

		newStore.Write( (void *) Buf, BytesLeft );

		BytesToGo -= BytesLeft;
	}

	if ( ( _wunlink( FileDataName.c_str() ) != 0 ) && ( errno != ENOENT ) )
	{
		return NUTSError( 0x10D, L"Failed to delete old data during Replace" );
	}

	newStore.KeepAs( FileDataName );

	return NUTS_SUCCESS;
}