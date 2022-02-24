#include "stdafx.h"

#include "ISOFuncs.h"
#include "ISORawSectorSource.h"
#include "libfuncs.h"
#include "SourceFunctions.h"
#include "TempFile.h"
#include "NUTSError.h"
#include "NUTS.h"

#include <algorithm>
#include <process.h>

HWND hJobWindow;
WORD JobStageBase;

void SetISOHints( DataSource *pSource, bool IsEOF, bool IsEOR )
{
	ISOWriteHint *hint;

	pSource->WriteHints.clear();

	DSWriteHint Hint;

	Hint.HintName = L"CDROMXA_SubHeader";
	
	hint = (ISOWriteHint *) Hint.Hint;

	hint->IsEOF = IsEOF;
	hint->IsEOR = IsEOR;

	pSource->WriteHints.push_back( Hint );
}

void ISOStrTerm( BYTE *p, WORD l )
{
	p[ l ] = 0;

	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		if ( p[ i ] != 0x20 )
		{
			return;
		}
		else
		{
			p[ i ] = 0x0;
		}
	}
}

void ISOStrStore( BYTE *p, BYTE *s, WORD ml )
{
	// This copies the length of the string onto the dest buffer, which is a fixed size given by ml.
	// The buffer is prefilled with spaces to pad. The use of memcpy ensures no terminating zero.
	WORD sl = rstrnlen( s, ml );

	memset( p, 0x20, ml );

	memcpy( p, s, sl );
}

void JolietStrTerm( BYTE *jp, WORD jl )
{
	WORD *p = (WORD *) jp;

	WORD l  = jl >> 1;

	p[ l ] = 0;

	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		if ( p[ i ] != 0x0020 )
		{
			break;
		}
		else
		{
			p[ i ] = 0x0;
		}
	}

	// Need to swap the bytes now
	for ( WORD i = (l - 1); i != 0xFFFF; i-- )
	{
		WORD v = p[ i ];

		v = ( ( v & 0xFF00 ) >> 8 ) | ( ( v & 0xFF ) << 8 );

		p[ i ] = v;
	}

	BYTE *pUTF = (BYTE *) AString( (WCHAR *) jp );

	rstrncpy( jp, pUTF, l );
}

bool ISOSectorSort( ISOSector &a, ISOSector &b )
{
	if ( a.ID < b.ID )
	{
		return true;
	}

	return false;
}

void PrepareISOSectors( ISOSectorList &Sectors )
{
	// First sort the sector list, in case it's not in order.
	std::sort( Sectors.begin(), Sectors.end(), ISOSectorSort );

	// Now assign a shift to the ordered list
	DWORD Shift = 1;

	for ( ISOSectorList::iterator iSector = Sectors.begin(); iSector != Sectors.end(); iSector++ )
	{
		iSector->Shift = Shift++;
	}
}

std::map<DWORD,DWORD> RemappedSectors;

DWORD RemapSector( DWORD OldSector, ISOSectorList &Sectors, BYTE IsoOp )
{
	if ( RemappedSectors.find( OldSector ) != RemappedSectors.end() )
	{
		return RemappedSectors[ OldSector ];
	}

	DWORD NewSector = OldSector;

	for ( ISOSectorList::iterator iSector = Sectors.begin(); iSector != Sectors.end(); iSector++ )
	{
		if ( IsoOp == ISO_RESTRUCTURE_ADD_SECTORS )
		{
			if ( OldSector >= iSector->ID )
			{
				NewSector = OldSector + iSector->Shift;
			}
		}

		if ( IsoOp == ISO_RESTRUCTURE_REM_SECTORS )
		{
			if ( OldSector >= iSector->ID )
			{
				NewSector = OldSector - iSector->Shift;
			}
		}
	}

	RemappedSectors[ OldSector ] = NewSector;

	return NewSector;
}

// In theory this can be used to calculate a percentage. In reality, there can be up to 8 path tables in High Sierra, and none of them in agreement.
DWORD PathTableEntries = 0;
DWORD ProcessedDirs    = 0;

static WORD RecursedDirs = 0;

int RemapDirectoryRecord( DataSource *pSource, CTempFile &WorkSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD DirSector, DWORD DirSize, DWORD SectorSize, DWORD FSID )
{
	AutoBuffer SectorBuffer( SectorSize );

	bool NeedRead = false;

	DWORD SectorPos = 0;
	DWORD DirPos  = 0;
	DWORD DirSect = DirSector;

	RecursedDirs++;

	if ( RecursedDirs > 50 )
	{
		return NUTSError( 0x800, L"Too many directories deep - restructure aborted" );
	}

	if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	while ( DirPos < DirSize )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			SectorPos += SectorSize;
			DirPos  = SectorPos;

			NeedRead = false;
		}

		if ( DirPos >= DirSize ) { break; }

		BYTE *pEnt = (BYTE *) SectorBuffer;

		pEnt = & pEnt[ DirPos - SectorPos ];

		BYTE DE_Len = pEnt[ 0 ];

		if ( DE_Len != 0 )
		{
			// Remap the extent
			DWORD Extent = * (DWORD *) &pEnt[ 2 ];

			DWORD NewExtent = RemapSector( Extent, Sectors, IsoOp );

			* (DWORD *) &pEnt[ 2 ] = NewExtent;

			WBEDWORD( &pEnt[ 6 ], NewExtent );

			BYTE Flags = pEnt[ 25 ];

			if ( FSID == FSID_ISOHS ) { Flags = pEnt[ 24 ]; }

			if (Flags & 2 ) {
				// Subdirectory! Recurse this
				DWORD ThisDirSize = * (DWORD *) &pEnt[ 10 ];

				bool SkipDir = false;

				// Don't try to recurse into . and .. - that way bad juju lies. Note that directory entries *should* use a very restrcited
				// character set (A-Z, 0-9, _) and thus 0x03 - 0x40 inclusive should be invalid. But you never know - there's nothing stopping
				// a CD masterer from breaking this rule.
				if ( ( pEnt[ 32 ] == 1 ) && ( pEnt[ 33 ] < 2 ) )
				{
					SkipDir = true;
				}

				if ( !SkipDir )
				{
					if ( RemapDirectoryRecord( pSource, WorkSource, Sectors, IsoOp, Extent, ThisDirSize, SectorSize, FSID ) != DS_SUCCESS )
					{
						return -1;
					}
				}
			}

			DirPos += DE_Len ;

			if ( DE_Len & 1 ) { DirPos++; }
		}
		
		if ( ( DE_Len == 0 ) || ( ( DirPos - SectorPos ) >= SectorSize ) )
		{
			NeedRead = true;

			WorkSource.Seek( DirSect * SectorSize );
			WorkSource.Write( (BYTE *) SectorBuffer, SectorSize );

			DirSect++;
		}
	}

	RecursedDirs--;

	ProcessedDirs++;

	SendMessage( GetDlgItem( hJobWindow, IDC_JOB_PROGRESS ), PBM_SETPOS, Percent( JobStageBase + 1, JobStageBase + 3, ProcessedDirs, PathTableEntries, false ), 0 );

	return DS_SUCCESS;
}

int RemapPathTable( DataSource *pSource, CTempFile &WorkSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD TableSector, DWORD TableSize, DWORD SectorSize, bool MTable )
{
	AutoBuffer SectorBuffer( SectorSize );

	bool NeedRead = false;

	DWORD SectorPos = 0;

	SendMessage( GetDlgItem( hJobWindow, IDC_JOB_PROGRESS ), PBM_SETPOS, Percent( JobStageBase + 0, JobStageBase + 3, 0, 0, false ), 0 );

	PathTableEntries = 0;

	// We're gonna have to read the whole thing in, because stupidly ISO allowed path table entries to span across sector boundaries.
	CTempFile PathTable;
	DWORD     TableSects = TableSize / SectorSize;

	if ( TableSize % SectorSize ) { TableSects++; }

	PathTable.Seek( 0 );

	for ( DWORD TableSect = 0; TableSect < TableSects; TableSect++ )
	{
		if ( pSource->ReadSectorLBA( TableSector + TableSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		PathTable.Write( (BYTE *) SectorBuffer, SectorSize );
	}

	BYTE  TableEntry[ 256 ];
	DWORD TablePos = 0;

	while ( TablePos < TableSize )
	{
		PathTable.Seek( TablePos );
		PathTable.Read( TableEntry, 256 );

		BYTE DI_Len = TableEntry[ 0 ];
		BYTE E_Len  = 8 + DI_Len;

		if ( DI_Len & 1 ) { E_Len++; }

		if ( DI_Len != 0 )
		{
			DWORD Extent = * (DWORD *) &TableEntry[ 2 ];

			if ( MTable ) { Extent = BEDWORD( &TableEntry[ 2 ] ); }

			Extent = RemapSector( Extent, Sectors, IsoOp );

			* (DWORD *) &TableEntry[ 2 ] = Extent;

			if ( MTable ) { WBEDWORD( &TableEntry[ 2 ], Extent ); }

			PathTableEntries++;
		}

		PathTable.Seek( TablePos );
		PathTable.Write( TableEntry, E_Len );
		
		TablePos += E_Len;
	}

	PathTable.Seek( 0 );
	WorkSource.Seek( TableSector * SectorSize );

	for ( DWORD TableSect = 0; TableSect < TableSects; TableSect++ )
	{
		PathTable.Read( (BYTE *) SectorBuffer, SectorSize );
		WorkSource.Write( (BYTE *) SectorBuffer, SectorSize );
	}

	return DS_SUCCESS;
}

int AdjustISOStructures( DataSource *pSource, CTempFile &WorkSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD SectorSize, DWORD FSID )
{
	// There are 3 things we need to do here:
	// 1) Adjust all of the volume descriptors to point to the adjusted sectors
	// 2) Adjust all path tables to point to the adjusted sectors
	// 3) Adjust the directory tree to point to the adjsuted sectors, in each volume descriptor.

	// Notes: ISO9660 allows 1 main table and 1 optional table for each of the L-Table and M-Table entries.
	//        High Sierra allows 1 main table and _3_ optional tables for each of L- and M-Tables.

	// Multiple volume descriptors may refer to the same sectors. It is therefore important we track which sectors we've already adjusted.
	RemappedSectors.clear();

	DWORD StartSector = 0x10;

	AutoBuffer SectorBuffer( SectorSize );

	while ( ( StartSector * SectorSize ) < pSource->PhysicalDiskSize )
	{
		if ( pSource->ReadSectorLBA( StartSector, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Adjusting volume descriptors..." ).c_str() );

		BYTE DescByte = SectorBuffer[ 0x000 ];

		if ( FSID == FSID_ISOHS ) {
			DescByte = SectorBuffer[ 0x008 ];
		}

		if ( DescByte == 0xFF )
		{
			// No more descriptors
			break;
		}

		if ( ( DescByte == 0x00 ) || ( DescByte == 0x03 ) )
		{
			// Boot record. Nah.
			StartSector++;

			continue;
		}

		// If we're here, we've got one.
		BYTE *p = (BYTE *) SectorBuffer;

		DWORD *pTables = (DWORD *) &p[ 140 ];
		BYTE   nTables = 4;
		BYTE   mSwitch = 2;

		DWORD  TableSize = * (DWORD *) &p[ 132 ];

		if ( FSID == FSID_ISOHS )
		{
			pTables = (DWORD *) &p[ 148 ];
			nTables = 8;
			mSwitch = 4;

			TableSize = * (DWORD *) &p[ 140 ];
		}

		SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Adjusting path tables..." ).c_str() );
		
		for ( BYTE t = 0; t < nTables; t++ )
		{
			DWORD TableSect = pTables[ t ];

			if ( t >= mSwitch ) { TableSect = BEDWORD( (BYTE *) &pTables[ t ] ); }

			if ( TableSect != 0 )
			{
				DWORD NewTableSect = RemapSector( TableSect, Sectors, IsoOp );

				pTables[ t ] = NewTableSect;

				if ( t >= mSwitch ) { WBEDWORD( (BYTE *) &pTables[ t ], NewTableSect ); }

				if ( RemapPathTable( pSource, WorkSource, Sectors, IsoOp, TableSect, TableSize, SectorSize, ( t >= mSwitch ) ) != DS_SUCCESS )
				{
					return -1;
				}
			}
		}

		// Fix up the location of the root directory in the super root
		BYTE *pSuperRoot = & p[ 156 ];

		if ( FSID == FSID_ISOHS )
		{
			pSuperRoot = & p[ 180 ];
		}

		DWORD RootSect = * (DWORD *) &pSuperRoot[ 2 ];

		DWORD NewRootSect = RemapSector( RootSect, Sectors, IsoOp );

		* (DWORD *) &pSuperRoot[ 2 ] = NewRootSect;

		WBEDWORD( &pSuperRoot[ 6 ], NewRootSect );

		DWORD DirSize = * (DWORD *) &pSuperRoot[ 10 ];

		ProcessedDirs = 0;

		SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Adjusting directory trees...." ).c_str() );

		if ( RemapDirectoryRecord( pSource, WorkSource, Sectors, IsoOp, RootSect, DirSize, SectorSize, FSID ) != DS_SUCCESS )
		{
			return -1;
		}

		// Fix up the new volume size
		BYTE *pSz = &p[ 80 ];

		if ( FSID == FSID_ISOHS )
		{
			pSz = &p[ 88 ];
		}

		DWORD SpaceSize = * (DWORD *) pSz;

		SpaceSize += Sectors.size();

		* (DWORD *) pSz = SpaceSize;
		WBEDWORD( &pSz[ 4 ], SpaceSize );

		// Write the Descriptor back
		WorkSource.Seek( StartSector * SectorSize );
		WorkSource.Write( (BYTE *) SectorBuffer, SectorSize );

		StartSector++;
	}

	return DS_SUCCESS;
}

int ISORestructure( DataSource *pSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD SectorSize, DWORD FSID )
{
	SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Duplicating data..." ).c_str() );

	CTempFile WorkSource;

	// Copy the entire image to a work area
	SourceToTemp( pSource, WorkSource );

	// Adjust ISO structures
	if ( AdjustISOStructures( pSource, WorkSource, Sectors, IsoOp, SectorSize, FSID ) != DS_SUCCESS )
	{
		return -1;
	}

	// Selectively copy the data back
	DWORD DestSectNum   = 0;
	DWORD NumSects      = ( WorkSource.Ext() + ( SectorSize - 1 ) ) / SectorSize;

	ISOSectorList::iterator iSector = Sectors.begin();

	SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Synchronizing data..." ).c_str() );

	AutoBuffer SectorBuffer( SectorSize );

	// Try and get this as close to 128K as the sector size allows
	DWORD KiloSize = ( 131072 / SectorSize ) * SectorSize;

	AutoBuffer OutBuffer( KiloSize );
	BYTE      *pOutBuffer = (BYTE *) OutBuffer;

	DWORD KiloPtr = 0;
	DWORD RawPtr  = 0;

	for ( DWORD Sect = 0; Sect < NumSects; Sect++ )
	{
		bool CopySector = false;
		bool AddSector  = false;

		if ( IsoOp == ISO_RESTRUCTURE_REM_SECTORS )
		{
			if ( ( iSector == Sectors.end() ) || ( Sect < iSector->ID ) )
			{
				CopySector = true;
			}
		}

		if ( IsoOp == ISO_RESTRUCTURE_ADD_SECTORS )
		{
			CopySector = true;

			if ( ( iSector != Sectors.end() ) && ( Sect == iSector->ID ) )
			{
				AddSector = true;
			}
		}

		if ( AddSector )
		{
			memset( (BYTE *) SectorBuffer, 0, SectorSize );

			memcpy( &pOutBuffer[ KiloPtr ], (BYTE *) SectorBuffer, SectorSize );

			KiloPtr += SectorSize;

			if ( KiloPtr == KiloSize )
			{
				if ( pSource->WriteRaw( RawPtr, KiloSize, pOutBuffer ) != DS_SUCCESS )
				{
					return -1;
				}

				RawPtr += KiloSize;
				KiloPtr = 0;
			}

			DestSectNum++;
		}

		if ( CopySector )
		{
			WorkSource.Seek( Sect * SectorSize );
			WorkSource.Read( (BYTE *) SectorBuffer, SectorSize );

			memcpy( &pOutBuffer[ KiloPtr ], (BYTE *) SectorBuffer, SectorSize );

			KiloPtr += SectorSize;

			if ( KiloPtr == KiloSize )
			{
				if ( pSource->WriteRaw( RawPtr, KiloSize, pOutBuffer ) != DS_SUCCESS )
				{
					return -1;
				}

				RawPtr += KiloSize;
				KiloPtr = 0;
			}

			DestSectNum++;
		}

		if ( ( iSector != Sectors.end() ) && ( Sect >= iSector->ID ) )
		{
			iSector++;
		}

		if ( ( Sect % 500 ) == 0 )
		{
			SendMessage( GetDlgItem( hJobWindow, IDC_JOB_PROGRESS ), PBM_SETPOS, Percent( JobStageBase + 2, JobStageBase + 3, Sect, NumSects, false ), 0 );
		}
	}

	if ( KiloPtr > 0 )
	{
		if ( pSource->WriteRaw( RawPtr, KiloPtr, pOutBuffer ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	QWORD NewSize = DestSectNum; NewSize *= (QWORD) SectorSize;

	pSource->Truncate( NewSize );

	SendMessage( GetDlgItem( hJobWindow, IDC_JOB_PROGRESS ), PBM_SETPOS, 100, 0 );

	return DS_SUCCESS;
}

static ISOJob CurrentISOJob;
static HANDLE hJobThread;
static DWORD  dwthreadid;

unsigned int __stdcall JobThread(void *param)
{
	switch ( CurrentISOJob.IsoOp )
	{
		case ISO_RESTRUCTURE_ADD_SECTORS:
		case ISO_RESTRUCTURE_REM_SECTORS:
			{
				CurrentISOJob.Result = ISORestructure(
					(DataSource *) CurrentISOJob.pSource,
					CurrentISOJob.Sectors,
					CurrentISOJob.IsoOp,
					CurrentISOJob.SectorSize,
					CurrentISOJob.FSID
				);
			}
			break;

		case ISO_JOB_SCAN_SIZE:
			{
				ScanRealImageSize( (DataSource *) CurrentISOJob.pSource, CurrentISOJob.pVolDesc, CurrentISOJob.FSID );

				CurrentISOJob.Result = 0;
			}
			break;

		default:
			break;
	}

	return 0;
}

INT_PTR CALLBACK ProgressDialog(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ( uMsg == WM_INITDIALOG )
	{
		hJobWindow = hwndDlg;

		SendMessage(GetDlgItem(hwndDlg, IDC_JOB_PROGRESS), PBM_SETRANGE32, 0, 100);
		SendMessage(GetDlgItem(hwndDlg, IDC_JOB_PROGRESS), PBM_SETPOS, 0, 0);

		switch ( CurrentISOJob.IsoOp )
		{
			case ISO_RESTRUCTURE_ADD_SECTORS:
			case ISO_RESTRUCTURE_REM_SECTORS:
				SendMessage( hwndDlg, WM_SETTEXT, 0, (LPARAM) std::wstring( L"Reorganising ISO Image, Please Wait..." ).c_str() );
				break;

			case ISO_JOB_REMOVE_JOLIET:
				SendMessage( hwndDlg, WM_SETTEXT, 0, (LPARAM) std::wstring( L"Removing Joliet Structures, Please Wait..." ).c_str() );
				break;

			case ISO_JOB_ADD_JOLIET:
				SendMessage( hwndDlg, WM_SETTEXT, 0, (LPARAM) std::wstring( L"Adding Joliet Structures, Please Wait..." ).c_str() );
				break;

			case ISO_JOB_SCAN_SIZE:
				SendMessage( hwndDlg, WM_SETTEXT, 0, (LPARAM) std::wstring( L"Scanning Disc Image to Determine Size, Please Wait..." ).c_str() );
				SendMessage( GetDlgItem( hJobWindow, IDC_JOB_NAME ), WM_SETTEXT, 0, (LPARAM) std::wstring( L"Scanning data structures..." ).c_str() );
				break;
		}

		hJobThread = (HANDLE) _beginthreadex(NULL, NULL, JobThread, NULL, NULL, (unsigned int *) &dwthreadid);

		SetTimer( hwndDlg, 0x150, 1000, NULL );

		return TRUE;
	}

	if ( ( uMsg == WM_TIMER ) && ( hJobThread != NULL ) )
	{
		if ( WaitForSingleObject( hJobThread, 0 ) == WAIT_OBJECT_0 )
		{
			KillTimer( hwndDlg, 0x150 );

			CloseHandle( hJobThread );

			if ( CurrentISOJob.Result != NUTS_SUCCESS )
			{
				NUTSError::Report( L"ISO Image Adjustment", hwndDlg );
			}

			EndDialog( hwndDlg, 0 );
		}
	}

	return FALSE;
}

int PerformISOJob( ISOJob *pJob )
{
	CurrentISOJob = *pJob;

	hJobThread = NULL;

	JobStageBase = 0;

	if ( ( CurrentISOJob.IsoOp == ISO_JOB_ADD_JOLIET ) || ( CurrentISOJob.IsoOp == ISO_JOB_REMOVE_JOLIET ) )
	{
		JobStageBase = 1;
	}

	DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_ISO_PROGRESS ), hMainWnd, ProgressDialog, NULL );

	return CurrentISOJob.Result;
}

int ISOExtendDataArea( DataSource *pSource, DWORD SectorSize, DWORD Sectors, DWORD FSID )
{
	DWORD SectorID = 16;

	AutoBuffer Sector( SectorSize );

	while ( ( SectorID * SectorSize ) < pSource->PhysicalDiskSize )
	{
		if ( pSource->ReadSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		BYTE *pSector = (BYTE *) Sector;

		BYTE *pID   = &pSector[ 0 ];
		BYTE *pSize = &pSector[ 80 ];

		if ( FSID == FSID_ISOHS )
		{
			pID   = &pSector[ 8 ];
			pSize = &pSector[ 88 ];
		}

		if ( *pID == 1 )
		{
			DWORD VolSize = LEDWORD( pSize );

			VolSize += Sectors;

			WLEDWORD( pSize, VolSize );
			WBEDWORD( &pSize[ 4 ], VolSize );
		}

		if ( *pID == 0xFF )
		{
			break;
		}

		SetISOHints( pSource, false, true );

		if ( pSource->WriteSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		SectorID++;
	}

	return 0;
}

int ISOSetRootSize( DataSource *pSource, DWORD SectorSize, DWORD RootSize, DWORD FSID )
{
	DWORD SectorID = 16;

	AutoBuffer Sector( SectorSize );

	while ( ( SectorID * SectorSize ) < pSource->PhysicalDiskSize )
	{
		if ( pSource->ReadSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		BYTE *pSector = (BYTE *) Sector;

		BYTE *pID   = &pSector[ 0 ];
		BYTE *pSize = &pSector[ 80 ];

		if ( FSID == FSID_ISOHS )
		{
			pID   = &pSector[ 8 ];
			pSize = &pSector[ 88 ];
		}

		if ( *pID == 1 )
		{
			BYTE *pSuperRoot = &pSector[ 156 ];

			if ( FSID == FSID_ISOHS )
			{
				pSuperRoot = &pSector[ 180 ];
			}

			* (DWORD *) &pSuperRoot[ 10 ] = RootSize;
			WBEDWORD( &pSuperRoot[ 14 ], RootSize );
		}

		if ( *pID == 0xFF )
		{
			break;
		}

		SetISOHints( pSource, false, true );

		if ( pSource->WriteSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		SectorID++;
	}

	return DS_SUCCESS;
}


int ISOSetDirSize( DataSource *pSource, DWORD SectorSize, DWORD DirSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, DWORD FSID )
{
	AutoBuffer SectorBuffer( SectorSize );

	bool NeedRead = false;

	DWORD SectorPos = 0;
	DWORD DirPos  = 0;
	DWORD DirSect = ParentSector;

	if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	while ( DirPos < ParentLength )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			SectorPos += SectorSize;
			DirPos  = SectorPos;

			NeedRead = false;
		}

		if ( DirPos >= ParentLength ) { break; }

		BYTE *pEnt = (BYTE *) SectorBuffer;

		pEnt = & pEnt[ DirPos - SectorPos ];

		BYTE DE_Len = pEnt[ 0 ];

		if ( DE_Len != 0 )
		{
			DWORD Extent = * (DWORD *) &pEnt[ 2 ];

			if ( Extent == DirSector )
			{
				* (DWORD *) &pEnt[ 10 ] = DirSize;

				WBEDWORD( &pEnt[ 14 ], DirSize );
			}

			DirPos += DE_Len ;

			if ( DE_Len & 1 ) { DirPos++; }
		}
		
		if ( ( DE_Len == 0 ) || ( ( DirPos - SectorPos ) >= SectorSize ) )
		{
			NeedRead = true;

			SetISOHints( pSource, ( DirPos == ParentLength), ( DirPos == ParentLength) );

			if ( pSource->WriteSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			DirSect++;
		}
	}

	return DS_SUCCESS;
}

int ISOSetSubDirParentSize( DataSource *pSource, DWORD SectorSize, DWORD DirSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, DWORD FSID )
{
	AutoBuffer SectorBuffer( SectorSize );

	bool NeedRead = false;

	DWORD SectorPos = 0;
	DWORD DirPos  = 0;
	DWORD DirSect = DirSector;

	if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	while ( DirPos < ParentLength )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			SectorPos += SectorSize;
			DirPos  = SectorPos;

			NeedRead = false;
		}

		if ( DirPos >= ParentLength ) { break; }

		BYTE *pEnt = (BYTE *) SectorBuffer;

		pEnt = & pEnt[ DirPos - SectorPos ];

		BYTE DE_Len = pEnt[ 0 ];

		if ( DE_Len != 0 )
		{
			DWORD Extent = * (DWORD *) &pEnt[ 2 ];

			if ( Extent == ParentSector )
			{
				* (DWORD *) &pEnt[ 10 ] = DirSize;

				WBEDWORD( &pEnt[ 14 ], DirSize );
			}

			DirPos += DE_Len ;

			if ( DE_Len & 1 ) { DirPos++; }
		}
		
		if ( ( DE_Len == 0 ) || ( ( DirPos - SectorPos ) >= SectorSize ) )
		{
			NeedRead = true;

			SetISOHints( pSource, ( DirPos == ParentLength ), ( DirPos == ParentLength ) );

			if ( pSource->WriteSectorLBA( DirSect, (BYTE *) SectorBuffer, SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			DirSect++;
		}
	}

	return DS_SUCCESS;
}

void ScanRealImageSizeDir( DataSource *pSource, ISOVolDesc *pVolDesc, DWORD &LastSector, DWORD DirExtent, DWORD DirSize, DWORD FSID )
{
	AutoBuffer SectorBuffer( pVolDesc->SectorSize );

	bool NeedRead = false;

	DWORD SectorPos = 0;
	DWORD DirPos  = 0;
	DWORD DirSect = DirExtent;

	if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, pVolDesc->SectorSize ) != DS_SUCCESS )
	{
		return;
	}

	while ( DirPos < DirSize )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) SectorBuffer, pVolDesc->SectorSize ) != DS_SUCCESS )
			{
				return;
			}

			SectorPos += pVolDesc->SectorSize;
			DirPos  = SectorPos;

			NeedRead = false;
		}

		if ( DirPos >= DirSize ) { break; }

		BYTE *pEnt = (BYTE *) SectorBuffer;

		pEnt = & pEnt[ DirPos - SectorPos ];

		BYTE DE_Len = pEnt[ 0 ];

		if ( DE_Len != 0 )
		{
			DWORD Extent = * (DWORD *) &pEnt[  2 ];
			DWORD Size   = * (DWORD *) &pEnt[ 10 ];
			DWORD SSize  = ( Size + ( pVolDesc->SectorSize -1 ) ) / pVolDesc->SectorSize;

			BYTE Flags = pEnt[ 25 ];

			if ( FSID == FSID_ISOHS )
			{
				Flags = pEnt[ 24 ];
			}

			if ( ( Extent + SSize ) > LastSector )
			{
				LastSector = Extent + SSize;
			}

			_ASSERT( LastSector < 0x60000 );

			if ( Flags & 2 )
			{
				// Don't recurse into . and ..
				bool SkipFile = false;

				if ( pEnt[ 32 ] == 1 )
				{
					if ( pEnt[ 33 ] < 2 )
					{
						SkipFile = true;
					}
				}

				if ( !SkipFile )
				{
					ScanRealImageSizeDir( pSource, pVolDesc, LastSector, Extent, Size, FSID );
				}
			}

			DirPos += DE_Len;

			if ( DE_Len & 1 ) { DirPos++; }
		}
		
		if ( ( DE_Len == 0 ) || ( ( DirPos - SectorPos ) >= pVolDesc->SectorSize ) )
		{
			NeedRead = true;

			DirSect++;
		}
	}
}

void ScanRealImageSize( DataSource *pSource, ISOVolDesc *pVolDesc, DWORD FSID )
{
	// The ISO and High Sierra Formats tend to lie a bit about the actual size of the image. As a reader,
	// nothing really cares - the files point to the data, so meh. But for writing, we do need to know
	// where the end of the disc actually is.

	DWORD LastSector = 16; // First VD starts here.

	DWORD VDSect = 16;

	AutoBuffer Sector( pVolDesc->SectorSize );

	while ( ( VDSect * pVolDesc->SectorSize ) < pSource->PhysicalDiskSize )
	{
		if ( pSource->ReadSectorLBA( VDSect, (BYTE *) Sector, pVolDesc->SectorSize ) != DS_SUCCESS )
		{
			return;
		}

		BYTE *pSector = (BYTE *) Sector;

		BYTE TypeByte = pSector[ 0 ];

		if ( FSID == FSID_ISOHS ) { TypeByte = pSector[ 8 ]; }

		// Consider this VD itself
		if ( VDSect > LastSector )
		{
			LastSector = VDSect;
		}
		
		if ( TypeByte == 0xFF )
		{
			pVolDesc->VolSize = LastSector;

			break;
		}

		if ( ( TypeByte == 0 ) || ( TypeByte > 2 ) )
		{
			VDSect++;

			continue;
		}

		// Consider the path tables first
		BYTE *pTables = &pSector[ 132 ];
		int   nTables = 4;
		int   sTable  = 2;

		if ( FSID == FSID_ISOHS )
		{
			pTables = &pSector[ 140 ];
			nTables = 8;
			sTable  = 4;
		}

		DWORD TableSize = LEDWORD( pTables );

		TableSize = ( TableSize + ( pVolDesc->SectorSize - 1 ) ) / pVolDesc->SectorSize;

		pTables += 8;

		for ( int t=0; t<nTables; t++ )
		{
			DWORD TableExtent = LEDWORD( pTables );

			if ( t >= sTable )
			{
				TableExtent = BEDWORD( pTables );
			}

			if ( ( TableExtent + TableSize ) > LastSector )
			{
				LastSector = TableExtent + TableSize;
			}

			pTables += 4;
		}

		// Recurse the directories

		BYTE *pEnt = &pSector[ 156 ];

		if ( FSID == FSID_ISOHS ) { pEnt = &pSector[ 180 ]; }

		DWORD DirExtent = LEDWORD( &pEnt[  2 ] );
		DWORD DirSize   = LEDWORD( &pEnt[ 10 ] );
		DWORD SDirSize  = ( DirSize + ( pVolDesc->SectorSize - 1 ) ) / pVolDesc->SectorSize;

		if ( ( DirExtent + SDirSize ) > LastSector )
		{
			LastSector = DirExtent + SDirSize;
		}

		ScanRealImageSizeDir( pSource, pVolDesc, LastSector, DirExtent, DirSize, FSID );

		BYTE *pVolSize = &pSector[ 80 ];

		if ( FSID == FSID_ISOHS )
		{
			pVolSize = &pSector[ 88 ];
		}

		WLEDWORD( &pVolSize[ 0 ], LastSector + 1 );
		WBEDWORD( &pVolSize[ 4 ], LastSector + 1 );

		SetISOHints( pSource, false, true );

		if ( pSource->WriteSectorLBA( VDSect, (BYTE *) Sector, pVolDesc->SectorSize ) != DS_SUCCESS )
		{
			return;
		}

		VDSect++;
	}

	pVolDesc->VolSize = LastSector + 1;
}

DWORD IsoCapacity;

INT_PTR CALLBACK CapacityDialog(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_INITDIALOG:
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"8cm CDROM - 184 MB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"CD-ROM - 650 MB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"CD-ROM - 703 MB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"CD-ROM - 791 MB (Non-Standard)" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"CD-ROM - 870 MB (Non-Standard)" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"8cm DVD-R - 1.1 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"8cm DVD-ROM - 1.3 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"DVD - 2.3 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"DVD - 4.3 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"DVD-R Dual Layer - 7.9 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"DVD-R Double Sided - 8.6 GB" ).c_str() );
		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_ADDSTRING, 0, (LPARAM) std::wstring( L"DVD Dual Layer Double Sided - 15.8 GB (Non-Standard)" ).c_str() );

		SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_SETCURSEL, 1, 0 );

		IsoCapacity = 333000;

		return FALSE;

	case WM_COMMAND:
		if ( wParam == (WPARAM) IDOK )
		{
			EndDialog( hwndDlg, 0 );
		}
		else if ( wParam == (WPARAM) IDCANCEL )
		{
			EndDialog( hwndDlg, 0xFF );
		}
		else if ( ( HIWORD( wParam ) == CBN_SELCHANGE ) && ( LOWORD( wParam ) == IDC_ISO_CAPACITY ) )
		{
			int Index = SendMessage( GetDlgItem( hwndDlg, IDC_ISO_CAPACITY ), CB_GETCURSEL, 0, 0);

			switch ( Index )
			{
			case 0:
				IsoCapacity = 94500; break;
			case 1:
				IsoCapacity = 333000; break;
			case 2:
				IsoCapacity = 360000; break;
			case 3:
				IsoCapacity = 405000; break;
			case 4:
				IsoCapacity = 445500; break;
			case 5:
				IsoCapacity = 600586; break;
			case 6:
				IsoCapacity = 714544; break;
			case 7:
				IsoCapacity = 1218960; break;
			case 8:
				IsoCapacity = 2295104; break;
			case 9:
				IsoCapacity = 4171712; break;
			case 10:
				IsoCapacity = 4590208; break;
			case 11:
				IsoCapacity = 8343424; break;
			default:
				IsoCapacity = 0; break;
			}
		}

		break;
	}

	return FALSE;
}

DWORD ISOChooseCapacity()
{
	IsoCapacity = 0;

	if ( DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_CHOOSE_ISO_CAPACITY ), hMainWnd, CapacityDialog, NULL ) == 0 )
	{
		return IsoCapacity;
	}

	return 0;
}

int ISOSetPathTableSize( DataSource *pSource, DWORD SectorSize, DWORD TableSize, DWORD FSID )
{
	DWORD SectorID = 16;

	AutoBuffer Sector( SectorSize );

	while ( ( SectorID * SectorSize ) < pSource->PhysicalDiskSize )
	{
		if ( pSource->ReadSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		BYTE *pSector = (BYTE *) Sector;

		BYTE *pID   = &pSector[ 0 ];
		BYTE *pSize = &pSector[ 132 ];

		if ( FSID == FSID_ISOHS )
		{
			pID   = &pSector[ 8 ];
			pSize = &pSector[ 141 ];
		}

		if ( *pID == 1 )
		{
			WLEDWORD( &pSize[ 0 ], TableSize );
			WBEDWORD( &pSize[ 4 ], TableSize );
		}

		if ( *pID == 0xFF )
		{
			break;
		}

		SetISOHints( pSource, false, true );

		if ( pSource->WriteSectorLBA( SectorID, (BYTE *) Sector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		SectorID++;
	}

	return 0;
}

void MakeISOFilename( NativeFile *pFile, BYTE *StringBuf )
{
	rstrncpy( StringBuf, pFile->Filename, 255 );

	BYTE Revs[ 12 ];

	if ( ( pFile->Flags & FF_Directory ) == 0 )
	{
		rsprintf( Revs, ";%d", pFile->Attributes[ ISOATTR_REVISION ] );
	}

	bool HasDot = false;
	WORD DotPos = 0;

	if ( pFile->Flags & FF_Extension )
	{
		DotPos = rstrnlen( StringBuf, 255 );

		rstrncat( StringBuf, (BYTE *) ".", 255 );
		rstrncat( StringBuf, pFile->Extension, 255 );

		HasDot = true;
	}

	BYTE *pPtr = StringBuf;
	BYTE p     = 0;

	while ( pPtr[p] != 0 )
	{
		if ( ( pPtr[p] >= '0' ) && ( pPtr[p] <= '9' ) )
		{
			p++;

			continue;
		}

		if ( ( pPtr[p] >= 'A' ) && ( pPtr[p] <= 'Z' ) )
		{
			p++;

			continue;
		}

		if ( pPtr[p] == '_' ) { p++; continue; }

		if ( ( pPtr[p] == '.' ) && ( p == DotPos ) ) { p++; continue; }

		if ( ( pPtr[p] >= 'a' ) && ( pPtr[p] <= 'z' ) )
		{
			pPtr[p] = pPtr[p] - ( 'a' - 'A' );

			p++;

			continue;
		}

		pPtr[ p ] = '_';

		p++;
	}

	if ( ( pFile->Flags & FF_Directory ) == 0 )
	{
		rstrncat( StringBuf, Revs, 255 );
	}
}