#pragma once

#include "ISO9660Directory.h"
#include "ISODefs.h"
#include "filesystem.h"

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
	int   Parent();
	bool  IsRoot();
	int   ReadFile(DWORD FileID, CTempFile &store);
	BYTE  *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE  *DescribeFile( DWORD FileIndex );
	BYTE  *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR *Identify( DWORD FileID );
	int   CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );
	LocalCommands GetLocalCommands( void );

	FileSystem *FileFilesystem( DWORD FileID );

	FileSystem *Clone( void )
	{
		ISO9660FileSystem *pFS = new ISO9660FileSystem( *this );

		return pFS;
	}

private:
	void ReadVolumeDescriptors( void );

private:
	ISO9660Directory *pISODir;

	ISOVolDesc PriVolDesc;
	ISOVolDesc JolietDesc;

	std::vector<DWORD> DirLBAs;
	std::vector<DWORD> DirLENs;

	std::string CDPath;

	bool HasJoliet;
};

