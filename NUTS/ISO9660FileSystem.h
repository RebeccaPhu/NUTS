#pragma once

#include "ISO9660Directory.h"
#include "ISODefs.h"
#include "ISOPathTable.h"
#include "filesystem.h"
#include "NUTSError.h"
#include "ISORawSectorSource.h"
#include "ISOSectorTypes.h"

#include <vector>

class ISO9660FileSystem :
	public FileSystem
{
public:
	ISO9660FileSystem( DataSource *pDataSource );
	ISO9660FileSystem( const ISO9660FileSystem &source );
	~ISO9660FileSystem(void);

	int   Init(void);
	int   ChangeDirectory( DWORD FileID );
	int	  CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	int   Parent();
	bool  IsRoot();
	int   ReadFile(DWORD FileID, CTempFile &store);
	int   ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj );
	int   WriteFile(NativeFile *pFile, CTempFile &store);
	int   Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt );
	BYTE  *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE  *DescribeFile( DWORD FileIndex );
	BYTE  *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR *Identify( DWORD FileID );
	int   CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	int   ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd );
	int   DeleteFile( DWORD FileID );
	int   DeleteFile( NativeFile *pFile );
	void  CommitOps( void );
	int   SetProps( DWORD FileID, NativeFile *Changes );
	int   ReplaceFile(NativeFile *pFile, CTempFile &store);
	int   Format_PreCheck( int FormatType, HWND hWnd );
	int   Format_Process( DWORD FT, HWND hWnd );
	int   RunTool( BYTE ToolNum, HWND ProgressWnd );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );
	LocalCommands GetLocalCommands( void );

	FileSystem *FileFilesystem( DWORD FileID );

	FSToolList GetToolsList( void );

	FileSystem *Clone( void )
	{
		ISO9660FileSystem *pFS = new ISO9660FileSystem( *this );

		return pFS;
	}

private:
	void ReadVolumeDescriptors( void );
	void WriteVolumeDescriptor( ISOVolDesc &VolDesc, DWORD Sector, DWORD FSID, bool Joliet );

	bool WritableTest();
	void RemoveJoliet();
	void AddJoliet();
	void JolietProcessDirectory( DWORD DirSector, DWORD DirSize, DWORD ParentSector, DWORD ParentSize, ISOPathTable *pJolietTable );
	void EnableWriting();
	void SetMaxCapacity();

	int  FilenameConflictCheck( int ExistsCode, NativeFile *pConflictor = nullptr );
	void MakeRockRidge( NativeFile *pFile );
	int  DirectoryResize( ISOSectorList *ExtraSectors = nullptr );
	void DirectoryShrink( );
	int  RemoveSectors();
	int  RenameIncomingDirectory( NativeFile *pDir );
	void RemapStack( ISOSectorList &Sectors, BYTE IsoOp );

	int  GatherDirs( FileSystem *pCloneFS, ISOPathTable *pScanTable, DWORD ThisExtent );
	int  CorrectDirs( FileSystem *pCloneFS, DWORD CurrentParent );
	int  RecordDirectorySectors( FileSystem *pCloneFS );

	void EditVolumeDescriptors( HWND hParentWnd );

private:
	ISO9660Directory *pISODir;
	ISOPathTable     *pPathTable;

	ISOVolDesc PriVolDesc;
	ISOVolDesc JolietDesc;

	std::string CDPath;

	bool HasJoliet;
	bool UsingJoliet;
	bool Writable;

	DWORD FreeSectors;
	DWORD ConflictedID;
	DWORD ScanEntries;
	DWORD DoneEntries;
	HWND  ScanWnd;

	ISOSectorList DeleteSectors;

	AutoBuffer *SectorsInUse;
	BYTE *pSectorsInUse;
};

