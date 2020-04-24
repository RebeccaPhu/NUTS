#pragma once
#include "DataSource.h"
#include "Directory.h"
#include "Defs.h"
#include "libfuncs.h"
#include "FormatWizard.h"
#include "TempFile.h"
#include "NUTSError.h"

#include <string>

#define ERROR_READONLY    0x00000020
#define ERROR_UNSUPPORTED 0x00000021
#define USE_STANDARD_WND  0x00000022
#define USE_CUSTOM_WND    0x00000023

class FileSystem
{
public:
	FileSystem(DataSource *pDataSource) {
		pSource	= pDataSource;

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
	}

	~FileSystem(void) {
		CloseHandle( hCancelFormat );
		CloseHandle( hCancelFree );
		CloseHandle( hToolEvent );

		free( pBlockMap );
	}

	virtual	int	ReadFile(DWORD FileID, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual	int	WriteFile(NativeFile *pFile, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual	int	ReplaceFile(NativeFile *pFile, CTempFile &store) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual int ChangeDirectory( DWORD FileID ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual int	Parent() {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual int	CreateDirectory( BYTE *Filename ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual FSHint Offer( BYTE *Extension )
	{
		NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );

		return FSHint( );
	}

	virtual DataSource *FileDataSource( DWORD FileID )
	{
		NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );

		return nullptr;
	}

	virtual FileSystem *FileFilesystem( DWORD FileID )
	{
		NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );

		return nullptr;
	}

	virtual	BYTE *GetTitleString( NativeFile *pFile = nullptr ) {
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

	virtual int DeleteFile( NativeFile *pFile, int FileOp ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual int Init(void) {
		pDirectory->ReadDirectory();

		return 0;
	}

	virtual DWORD GetEncoding(void )
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
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
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
		static WCHAR *MiscFile      = (WCHAR *) L"Container File";
		static WCHAR *Directory     = (WCHAR *) L"Directory";

		if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			return Directory;
		}

		switch ( pDirectory->Files[ FileID ].Icon )
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

		case FT_Directory:
			break;

		default:
			return NoneFile;
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
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	Directory	*pDirectory;

	DataSource	*pSource;

	//	Formatting functions where available:

	//	Gives a file system the opportunity to ask any additional questions or post information etc, before the format begins.
	virtual int	Format_PreCheck( int FormatType ) {
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	//	Run the actual format process. Intended to be run in a separate thread from the format wizard.
	virtual	int Format_Process( FormatType FT, HWND hWnd ) {
		PostMessage( hWnd, WM_FORMATPROGRESS, 200, 0);

		return 0;
	}

	virtual int CancelFormat( void )
	{
		SetEvent( hCancelFormat );

		return 0;
	}

	virtual int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
	{
		return NUTSError( ERROR_UNSUPPORTED, L"Operated not supported" );
	}

	virtual int CancelSpace( void )
	{
		SetEvent( hCancelFree );
		
		return 0;
	}

	virtual std::vector<AttrDesc> GetAttributeDescriptions( void )
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
	
	virtual int Rename( DWORD FileID, BYTE *NewName )
	{
		rstrncpy( pDirectory->Files[ FileID ].Filename, NewName, 256 );

		return pDirectory->WriteDirectory();
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

	DWORD FSID;
	DWORD EnterIndex;
	bool  IsRaw;
	DWORD Flags;

	FileSystem *pParentFS;

	bool UseResolvedIcons;
	bool HideSidecars;

	HWND hMainWindow;
	HWND hPaneWindow;

protected:
	HANDLE hCancelFormat;
	HANDLE hCancelFree;
	HANDLE hToolEvent;

	BYTE *pBlockMap;
};

