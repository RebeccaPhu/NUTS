#pragma once
#include "DataSource.h"
#include "Directory.h"
#include "Defs.h"
#include "libfuncs.h"
#include "TempFile.h"
#include "NUTSError.h"
#include "NestedImageSource.h"
#include "FOP.h"

#include <string>
#include <deque>

#define NUTS_SUCCESS       0x00000000
#define ERROR_READONLY     0x00000020
#define ERROR_UNSUPPORTED  0x00000021
#define USE_STANDARD_WND   0x00000022
#define USE_CUSTOM_WND     0x00000023
#define ASCIIFILE_REQUIRED 0x00000024

#define CDF_ENTER_AFTER    0x00000001
#define CDF_INSTALL_OP     0x00000002
#define CDF_MANUAL_OP      0x00000004
#define CDF_MERGE_DIR      0x00000008
#define CDF_RENAME_DIR     0x00000010


class FileSystem
{
public:
	FileSystem(DataSource *pDataSource) {
		AlternateOffsets.clear();

		TopicIcon = FT_MiscImage;

		pSource	= pDataSource;

		if ( pSource != nullptr )
		{
			DS_RETAIN( pSource );
		}

		FSID = FS_Null;

		EnterIndex = 0xFFFFFFFF;
		IsRaw      = false;

		hCancelFormat = CreateEvent( NULL, TRUE, FALSE, NULL );
		hCancelFree   = CreateEvent( NULL, TRUE, FALSE, NULL );
		hToolEvent    = CreateEvent( NULL, TRUE, FALSE, NULL );

		Flags      = 0;

		pBlockMap = (BYTE *) malloc( TotalBlocks );

		UseResolvedIcons = false;
		HideSidecars     = false;
		pParentFS        = nullptr;

		PreferredArbitraryExtension = (BYTE *) "IMG";

		FileOpsAction = AA_NONE;
	}

	virtual ~FileSystem(void) {
		CloseHandle( hCancelFormat );
		CloseHandle( hCancelFree );
		CloseHandle( hToolEvent );

		free( pBlockMap );

		if ( pSource != nullptr )
		{
			DS_RELEASE( pSource );
		}
	}

	virtual int ReadFile(DWORD FileID, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int WriteFile(NativeFile *pFile, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int ReplaceFile(NativeFile *pFile, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int ChangeDirectory( DWORD FileID ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int Parent() {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int CreateDirectory( NativeFile *pDir, DWORD CreateFlags ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual FSHint Offer( BYTE *Extension )
	{
		NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );

		return FSHint( );
	}

	virtual DataSource *FileDataSource( DWORD FileID )
	{
		CTempFile FileObj;

		if ( ReadFile( FileID, FileObj ) != DS_SUCCESS )
		{
			return nullptr;
		}

		FileObj.Keep();

		DataSource *pNewSource = new NestedImageSource( this, &pDirectory->Files[ FileID ], FileObj.Name() );

		return pNewSource;
	}

	virtual FileSystem *FileFilesystem( DWORD FileID )
	{
		return nullptr;
	}

	virtual	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags ) {
		static char *baseTitle = "FileSystem";

		return (BYTE *) baseTitle;
	}

	virtual	bool IsRoot() {
		return true;
	}

	virtual BYTE *DescribeFile( DWORD FileIndex )
	{
		static BYTE status[1] = { 0 };

		return status;
	}

	virtual	BYTE *GetStatusString( int FileIndex, int SelectedItems ) {
		static BYTE *baseStatus = (BYTE *) "Loaded";

		return baseStatus;
	}

	virtual int DeleteFile( DWORD FileID ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int DeleteFile( NativeFile *pFile )
	{
		if ( pDirectory != nullptr )
		{
			for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
			{
				if ( FilenameCmp( pFile, &*iFile ) )
				{
					return DeleteFile( iFile->fileID );
				}
			}
		}

		return NUTSError( 400, L"Couldn't find file to delete (software bug)" );
	}

	virtual int Init(void) {
		pDirectory->ReadDirectory();

		return 0;
	}

	virtual EncodingIdentifier GetEncoding(void )
	{
		return ENCODING_ASCII;
	}

	virtual int SetProps( DWORD FileID, NativeFile *Changes )
	{
		for ( BYTE i=0; i<16; i++ )
		{
			pDirectory->Files[ FileID ].Attributes[ i ] = Changes->Attributes[ i ];
		}

		pDirectory->WriteDirectory();
		pDirectory->ReadDirectory();

		return 0;
	}

	virtual int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
	{
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual WCHAR *Identify( DWORD FileID )
	{
		static WCHAR *BinaryFile    = (WCHAR *) L"Binary File";
		static WCHAR *ScriptFile    = (WCHAR *) L"Script";
		static WCHAR *AppFile       = (WCHAR *) L"Application";
		static WCHAR *DataFile      = (WCHAR *) L"Data File";
		static WCHAR *GraphicFile   = (WCHAR *) L"Graphic File";
		static WCHAR *SoundFile     = (WCHAR *) L"Sound File";
		static WCHAR *PrefFile      = (WCHAR *) L"Preferences";
		static WCHAR *TextFile      = (WCHAR *) L"Text File";
		static WCHAR *NoneFile      = (WCHAR *) L"Unknown File Type";
		static WCHAR *ArbitraryFile = (WCHAR *) L"Unspecified File Type";
		static WCHAR *CodeFile      = (WCHAR *) L"Executable Code";
		static WCHAR *SpoolFile     = (WCHAR *) L"Spooled Output";
		static WCHAR *BASICFile     = (WCHAR *) L"BASIC Program";
		static WCHAR *DiskFile      = (WCHAR *) L"Disk Image";
		static WCHAR *TapeFile      = (WCHAR *) L"Cassette Image";
		static WCHAR *HardFile      = (WCHAR *) L"Hard Drive Image";
		static WCHAR *CDFile        = (WCHAR *) L"CD/DVD Image";
		static WCHAR *MiscFile      = (WCHAR *) L"Container File";
		static WCHAR *Directory     = (WCHAR *) L"Directory";

		if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			return Directory;
		}

		switch ( pDirectory->Files[ FileID ].Type )
		{
		case FT_None:
			return NoneFile;

		case FT_Arbitrary:
			return ArbitraryFile;

		case FT_Binary:
			return BinaryFile;

		case FT_Code:
			return CodeFile;

		case FT_Script:
			return ScriptFile;

		case FT_Spool:
			return SpoolFile;

		case FT_Text:
			return TextFile;

		case FT_BASIC:
			return BASICFile;

		case FT_Data:
			return DataFile;

		case FT_Graphic:
			return GraphicFile;

		case FT_Sound:
			return SoundFile;

		case FT_Pref:
			return PrefFile;

		case FT_App:
			return AppFile;

		case FT_DiskImage:
			return DiskFile;

		case FT_TapeImage:
			return TapeFile;

		case FT_MiscImage:
			return MiscFile;

		case FT_HardImage:
			return HardFile;

		case FT_CDImage:
			return CDFile;

		case FT_Directory:
			break;

		default:
			return NoneFile;
		}

		if ( pDirectory->Files[ FileID ].Icon == FT_DiskImage )
		{
			return DiskFile;
		}

		if ( pDirectory->Files[ FileID ].Icon == FT_TapeImage )
		{
			return TapeFile;
		}

		if ( pDirectory->Files[ FileID ].Icon == FT_HardImage )
		{
			return HardFile;
		}

		if ( pDirectory->Files[ FileID ].Icon == FT_CDImage )
		{
			return CDFile;
		}

		return NoneFile;
	}

	virtual FSToolList GetToolsList( void )
	{
		FSToolList list;

		return list;
	}

	virtual int PreRunTool( BYTE ToolNum )
	{
		return USE_STANDARD_WND;
	}

	virtual int RunTool( BYTE ToolNum, HWND ProgressWnd )
	{
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	Directory	*pDirectory;

	DataSource	*pSource;

	//	Formatting functions where available:

	//	Gives a file system the opportunity to ask any additional questions or post information etc, before the format begins.
	virtual int	Format_PreCheck( int FormatType, HWND hWnd ) {
		return 0;
	}

	//	Run the actual format process. Intended to be run in a separate thread from the format wizard.
	virtual	int Format_Process( DWORD FT, HWND hWnd ) {
		PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

		return 0;
	}

	virtual int CancelFormat( void )
	{
		SetEvent( hCancelFormat );

		return 0;
	}

	virtual int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
	{
		return NUTSError( ERROR_UNSUPPORTED, L"Operation not supported" );
	}

	virtual int CancelSpace( void )
	{
		SetEvent( hCancelFree );
		
		return 0;
	}

	virtual std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr )
	{
		std::vector<AttrDesc> none;

		return none;
	}

	virtual std::vector<AttrDesc> GetFSAttributeDescriptions( void )
	{
		std::vector<AttrDesc> none;

		return none;
	}

	virtual int Refresh( void )
	{
		pDirectory->ReadDirectory();

		return 0;
	}
	
	virtual int Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt )
	{
		for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
		{
			bool Same = false;

			if ( ( rstrcmp( iFile->Filename, NewName ) ) && ( FileID != iFile->fileID ) )
			{
				if ( iFile->Flags & FF_Extension )
				{
					if ( ( rstrcmp( iFile->Extension, NewExt ) ) && ( FileID != iFile->fileID ) )
					{
						Same = true;
					}
				}
				else
				{
					Same = true;
				}
			}

			if ( Same )
			{
				return NUTSError( 0x206, L"An object with that name already exists" );
			}
		}

		pDirectory->Files[ FileID ].Filename = BYTEString( NewName );

		if ( pDirectory->Files[ FileID ].Flags & FF_Extension )
		{
			if ( rstrnlen( NewExt, 256 ) == 0 )
			{
				pDirectory->Files[ FileID ].Flags &= ~FF_Extension;
			}
			else
			{
				pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
			}
		}
		else
		{
			if ( rstrnlen( NewExt, 256 ) != 0 )
			{
				if ( Flags & FSF_Uses_Extensions )
				{
					if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
					{
						if ( ! ( Flags & FSF_NoDir_Extensions ) )
						{
							pDirectory->Files[ FileID ].Flags |= FF_Extension;
							pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
						}
					}
					else
					{
						pDirectory->Files[ FileID ].Flags |= FF_Extension;
						pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
					}
				}
			}
		}

		int r = pDirectory->WriteDirectory();

		if ( r == NUTS_SUCCESS )
		{
			r = pDirectory->ReadDirectory();
		}

		return r;
	}

	virtual void CancelTool( void )
	{
		SetEvent( hToolEvent );
	}

	virtual int SwapFile( DWORD FileID1, DWORD FileID2 )
	{
		NativeFile x = pDirectory->Files[ FileID1 ];

		pDirectory->Files[ FileID1 ] = pDirectory->Files[ FileID2 ];
		pDirectory->Files[ FileID2 ] = x;

		pDirectory->WriteDirectory();
		pDirectory->ReadDirectory();

		return 0;
	}

	virtual int MakeASCIIFilename( NativeFile *pFile )
	{
		for ( WORD i=0; i<pFile->Filename.length(); i++ )
		{
			pFile->Filename[ i ] &= 0x7F;

			if ( ( pFile->Filename[ i ] < 32 ) && ( pFile->Filename[ i ] != 0 ) )
			{
				pFile->Filename[ i ] = '_';
			}
		}

		pFile->EncodingID = ENCODING_ASCII;

		return 0;
	}

	virtual int ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
	{
		return 0;
	}

	virtual int ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj )
	{
		return 0;
	}

	virtual int EnhanceFileData( NativeFile *pFile )
	{
		return 0;
	}

	virtual LocalCommands GetLocalCommands( void )
	{
		LocalCommands cmds;

		cmds.HasCommandSet = false;

		return cmds;
	}

	virtual int ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
	{
		return 0;
	}

	virtual int MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store )
	{
		return 0;
	}

	virtual int WriteFork( WORD ForkID, CTempFile *pFork )
	{
		if ( ForkID == 0 )
		{
			Forks.clear();
		}

		Forks.push_back( pFork );

		return 0;
	}

	virtual int ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj )
	{
		return NUTSError( 0x207, L"Operation not supported" );
	}

	virtual int WriteCleanup( void )
	{
		// DO NOT DELETE THE FORKS! They are pointers only, and will be managed by FileOps!

		Forks.clear();
		Forks.shrink_to_fit();

		return 0;
	}

	virtual FileSystem *Clone( void )
	{
		return nullptr;
	}

	virtual void BeginOps( ActionID action )
	{
		FileOpsAction = action;
	}

	virtual void CommitOps( void )
	{
		FileOpsAction = AA_NONE;
	}

	FSIdentifier FSID;
	DWORD EnterIndex;
	bool  IsRaw;
	DWORD Flags;
	DWORD TopicIcon;

	FileSystem *pParentFS;

	bool UseResolvedIcons;
	bool HideSidecars;

	HWND hMainWindow;
	HWND hPaneWindow;

	std::deque<DWORD> AlternateOffsets;

	BYTEString PreferredArbitraryExtension;

	FOPTranslateFunction ProcessFOP;
	FOPLoadFSFunction    LoadFOPFS;

protected:
	HANDLE hCancelFormat;
	HANDLE hCancelFree;
	HANDLE hToolEvent;

	BYTE *pBlockMap;

	std::vector<CTempFile *> Forks;

	ActionID FileOpsAction;
};

