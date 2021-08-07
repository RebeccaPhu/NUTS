#pragma once

#include "../NUTS/FileSystem.h"

#include "CPMBlockMap.h"
#include "CPMDirectory.h"

#include "Defs.h"

class CPMFileSystem :
	public FileSystem
{
public:
	CPMFileSystem(DataSource *pDataSource, CPMDPB &cpmdpb) : FileSystem( pDataSource )
	{
		pMap = new CPMBlockMap( pDataSource );
		pDir = new CPMDirectory( pDataSource, pMap, cpmdpb );

		pDirectory = (Directory *) pDir;

		Flags = FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Uses_DSK | FSF_Uses_Extensions;
		FSID  = cpmdpb.FSID;
		dpb   = cpmdpb;

		DirSector  = 0;		
		SystemDisk = false;

		set.clear();

		CTrack     = 0xFFFFFFFF;
	}

	~CPMFileSystem(void)
	{
		delete pDir;
		delete pMap;
	}

public:
	virtual BYTE  *GetTitleString( NativeFile *pFile = nullptr );
	virtual BYTE  *DescribeFile( DWORD FileIndex );
	virtual BYTE  *GetStatusString( int FileIndex, int SelectedItems );

	virtual int   ReadFile( DWORD FileID, CTempFile &store );
	virtual int   WriteFile(NativeFile *pFile, CTempFile &store);
	virtual int   DeleteFile( DWORD FileID );
	virtual	int	  ReplaceFile(NativeFile *pFile, CTempFile &store);
	virtual int   Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	virtual DWORD GetEncoding(void ) { return dpb.Encoding; }

	virtual int   CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	virtual std::vector<AttrDesc> GetAttributeDescriptions( void );
	virtual int   SetProps( DWORD FileID, NativeFile *Changes );
	virtual std::vector<AttrDesc> GetFSAttributeDescriptions( void );
	virtual int  SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	virtual void SetShape()
	{
		DiskShape ds;

		ZeroMemory( &ds, sizeof(ds) );
		
		/* Fill this in so that DSK knows how big a sector is (should always be 0x200,
		   but you never know. */
		ds.SectorSize = dpb.SecSize;
		ds.Sectors    = dpb.SecsPerTrack;

		pSource->SetDiskShape( ds );
	}

	virtual int Init(void) {
		SetShape();

		/* Query the sector ID - if it's >= 0xC0 - it's a data disk. If it's 0xFFFF then something went wrong */ 
		SystemDisk = false;

		SectorIDSet set = pSource->GetTrackSectorIDs( 0, 0, false );

		if ( ( set.size() > 0 ) && ( set[ 0 ] < 0x00C0 ) )
		{
			DirSector += dpb.SysTracks * dpb.SecsPerTrack;

			SystemDisk = true;
		}

		pDir->DirSector = DirSector;

		/* Handy code to extract the system tracks */
		
		#if 1 == 0
		FILE *fSys = fopen("F:/cpm.sys", "wb");
		AutoBuffer buf( dpb.SecSize );

		for ( BYTE t=0; t<dpb.SysTracks; t++ )
		{
			for ( BYTE s=0; s<dpb.SecsPerTrack; s++ )
			{
				QWORD Offset = ( t * ( dpb.SecsPerTrack * dpb.SecSize ) ) + ( s * dpb.SecSize );

				ReadRawBySectors( Offset, dpb.SecSize, buf );

				fwrite( buf, 1, dpb.SecSize, fSys );
			}
		}

		fclose( fSys );
		#endif

		return pDirectory->ReadDirectory();
	}

	virtual bool GetCPMHeader( NativeFile *pFile, BYTE *pHeader )   { return true; }
	virtual int  ParseCPMHeader( NativeFile *pFile, BYTE *pHeader ) { return -1; }
	virtual bool IncludeHeader( BYTE *pHeader ) { return true; }

	virtual int  Format_PreCheck( int FormatType, HWND hWnd );

	virtual void CPMPreWriteCheck( NativeFile *pFile ) { return; }

public:
	CPMDirectory *pDir;

protected:
	CPMBlockMap  *pMap;
	CPMDPB       dpb;
	bool         SystemDisk;

private:

	DWORD        DirSector;

	int ReadExtent( DWORD LBA, DWORD Length, BYTE *Buffer );
	int WriteExtent( DWORD LBA, DWORD Length, BYTE *Buffer );

	DWORD		CTrack;

	SectorIDSet set;
};

