#include "StdAfx.h"

#include "../NUTS/CPM.h"
#include "AMSDOSFileSystem.h"
#include "AMSDOSDirectory.h"
#include "Defs.h"
#include "AmstradDLL.h"
#include "resource.h"

CPMDPB AMSDOS_DPB = {
	/* .ShortFSName  = */ "AMSDOS",
	/* .Encoding     = */ ENCODING_CPC,
	/* .FSFileType   = */ FILE_AMSTRAD,
	/* .SysTracks    = */ 2,
	/* .SecsPerTrack = */ 9,
	/* .SecSize      = */ 0x200,
	/* .FSID         = */ FSID_AMSDOS,
	/* .DirSecs      = */ 4,
	/* .ExtentSize   = */ 1024,
	/* .Flags        = */ CF_UseHeaders | CF_Force8bit | CF_SystemOptional,
	/* .HeaderSize   = */ 0x80,
};

AMSDOSFileSystem::AMSDOSFileSystem(DataSource *pDataSource) : CPMFileSystem( pDataSource, AMSDOS_DPB )
{
	/* Remove the directory object and create a new one */
	delete pDir;

	pDir = (CPMDirectory *) new AMSDOSDirectory( pDataSource, pMap, AMSDOS_DPB );

	pDirectory = (Directory *) pDir;

	Flags =
		FSF_Creates_Image | FSF_Formats_Image |
		FSF_SupportBlocks | FSF_SupportFreeSpace | FSF_Capacity |
		FSF_FixedSize | FSF_UseSectors | FSF_Uses_DSK | FSF_No_Quick_Format | FSF_Uses_Extensions;
}

bool AMSDOSFileSystem::IncludeHeader( BYTE *pHeader )
{
	WORD sum = 0;

	for ( BYTE i=0; i<66; i++ )
	{
		sum += pHeader[ i ];
	}

	WORD expected = * (WORD *) &pHeader[ 67 ];

	if ( sum == expected )
	{
		return false;
	}

	return true;
}

FSHint AMSDOSFileSystem::Offer( BYTE *Extension )
{
	SetShape();

	FSHint hint;

	hint.FSID       = FSID;
	hint.Confidence = 0;

	if ( Extension != nullptr )
	{
		if ( rstrnicmp( Extension, (BYTE *) "DSK", 3 ) )
		{
			hint.Confidence += 10;
		}
	}

	SectorIDSet set = pSource->GetTrackSectorIDs( 0, 0, false );

	/* CPC Data Disk */
	if ( ( set.size() > 0 ) && ( set[ 0 ] >= 0xC0 ) && ( set[ 0 ] <= 0xCA ) )
	{
		hint.Confidence += 10;
	}

	/* CPC System Disk */
	if ( ( set.size() > 0 ) && ( set[ 0 ] >= 0x40 ) && ( set[ 0 ] <= 0x4A ) )
	{
		hint.Confidence += 10;
	}

	if ( set.size() == 0 )
	{
		hint.Confidence = 0;
	}

	return hint;
}

int AMSDOSFileSystem::Format_Process( FormatType FT, HWND hWnd )
{
	WCHAR FormatMsg[ 256 ];

	static WCHAR *pDone = L"Format Complete";

	DiskShape shape;

	shape.Heads           = 1;
	shape.LowestSector    = 1;
	shape.Sectors         = 9;
	shape.SectorSize      = 0x200;
	shape.TrackInterleave = 0;
	shape.Tracks          = 40;

	pSource->StartFormat( shape );

	const BYTE SectorIDs[ 9 ] = { 0x01, 0x06, 0x02, 0x07, 0x03, 0x08, 0x04, 0x09, 0x05 };

	HRSRC hResource  = FindResource( hInstance, MAKEINTRESOURCE( IDR_CPM ), RT_RCDATA );
	HGLOBAL hMemory  = LoadResource( hInstance, hResource );
	DWORD dwSize     = SizeofResource( hInstance, hResource );
	BYTE  *pSYS      = (BYTE *) LockResource( hMemory );

	/* Low-level format */
	for ( BYTE t=0; t<40; t++ )
	{
		pSource->SeekTrack( t );

		TrackDefinition tr;

		tr.Density = DoubleDensity;
		tr.GAP1.Repeats = 80;
		tr.GAP1.Value   = 0x4E;

		for ( BYTE s=0; s<9; s++ )
		{
			if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			SectorDefinition sd;

			sd.GAP2PLL.Repeats = 12;
			sd.GAP2PLL.Value   = 0x00;

			sd.GAP2SYNC.Repeats = 3;
			sd.GAP2SYNC.Value   = 0xA1;

			sd.IAM      = 0xFE;
			sd.Track    = t;
			sd.Side     = 0;
			sd.SectorLength = 0x02; // 512 Bytes
			sd.IDCRC    = 0xFF;

			if ( SystemDisk )
			{
				sd.SectorID = 0x40 + SectorIDs[ s ];
			}
			else
			{
				sd.SectorID = 0xC0 + SectorIDs[ s ];
			}

			sd.GAP3.Repeats = 22;
			sd.GAP3.Value   = 0x4E;

			sd.GAP3PLL.Repeats = 12;
			sd.GAP3PLL.Value   = 0;
			
			sd.GAP3SYNC.Repeats = 3;
			sd.GAP3SYNC.Value   = 0xA1;

			sd.DAM = 0xFB;

			memset( sd.Data, 0xE5, 512 );

			if ( SystemDisk )
			{
				if ( t < 2 )
				{
					DWORD Offset = ( t * 9 * 0x200 ) + ( ( SectorIDs[ s ] - 1 ) * 0x200 );

					memcpy( sd.Data, &pSYS[ Offset ], 0x200 );
				}
			}

			sd.GAP4.Repeats     = 40;
			sd.GAP4.Value       = 0x4E;

			tr.Sectors.push_back( sd );

			swprintf_s( FormatMsg, 256, L"Formatting track %d sector %d", t, SectorIDs[ s ] );

			SendMessage( hWnd, WM_FORMATPROGRESS, Percent( t, 80, s, 9, false ), (LPARAM) FormatMsg );
		}
		
		tr.GAP5 = 0x4E;

		pSource->WriteTrack( tr );
	}

	pSource->EndFormat();

	SendMessage( hWnd, WM_FORMATPROGRESS, 100, (LPARAM) pDone );

	return 0;
}


std::vector<AttrDesc> AMSDOSFileSystem::GetAttributeDescriptions( void )
{
	std::vector<AttrDesc> Attrs = CPMFileSystem::GetAttributeDescriptions();

	AttrDesc Attr;

	/* Load address Hex, visible, disabled */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Executable address Hex, visible, disabled */
	Attr.Index = 6;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Exec address";
	Attrs.push_back( Attr );

    /* File Type. */
	Attr.Index = 7;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrFile;
	Attr.Name  = L"File Type";

	static std::wstring Names[ 3 ] = {
		L"BASIC", L"Protected", L"Binary"
	};

	for ( BYTE i=0; i<3; i++ )
	{
		AttrOption opt;

		opt.Name            = Names[ i ];
		opt.EquivalentValue = i;
		opt.Dangerous       = true;

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	Attr.Index = 8 ;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrDanger | AttrFile;
	Attr.Name  = L"Has header";

	Attrs.push_back( Attr );

	return Attrs;
}

int AMSDOSFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	for ( BYTE i=0; i<8; i++ )
	{
		pFile->Attributes[ i ] = Changes->Attributes[ i ];
	}

	if ( pFile->Attributes[ 8 ] == Changes->Attributes[ 8 ] )
	{
		int r = CPMFileSystem::SetProps( FileID, Changes );

		if ( r != DS_SUCCESS )
		{
			return -1;
		}

		BYTE SectorBuf[ 512 ];

		pSource->ReadRaw( pFile->Attributes[ 1 ] * 1024, 512, SectorBuf );

		GetCPMHeader( pFile, SectorBuf );

		return pSource->WriteRaw( pFile->Attributes[ 1 ] * 1024, 512, SectorBuf );
	}

	pFile->Attributes[ 8 ] = Changes->Attributes[ 8 ];

	NativeFile file = *pFile;

	/* Read, delete and rewrite so that the header data changes */
	CTempFile tmp;

	ReadFile( FileID, tmp );
	DeleteFile( FileID );
	WriteFile( &file, tmp );

	return 0;
}

bool AMSDOSFileSystem::GetCPMHeader( NativeFile *pFile, BYTE *pHeader )
{
	if ( pFile->Attributes[ 8 ] == 0x00000000 )
	{
		return false;
	}

	/* Fudge some values if this file is foreign */
	if ( pFile->FSFileType != FILE_AMSTRAD )
	{
		pFile->Attributes[ 5 ] = 0;
		pFile->Attributes[ 6 ] = 0;
		pFile->Attributes[ 7 ] = 2;
	}

	ZeroMemory( pHeader, 128 );

	ExportCPMDirectoryEntry( pFile, pHeader );

	* (WORD *) &pHeader[ 21 ] = (WORD) pFile->Attributes[ 5 ];
	* (WORD *) &pHeader[ 64 ] = (WORD) pFile->Attributes[ 5 ];
	* (WORD *) &pHeader[ 26 ] = (WORD) pFile->Attributes[ 6 ];
	* (WORD *) &pHeader[ 24 ] = (WORD) pFile->Length;

	pHeader[ 18 ] = (BYTE) pFile->Attributes[ 7 ];

	/* Calculate checksum on header */
	WORD sum = 0;

	for ( BYTE i=0; i<66; i++ )
	{
		sum += pHeader[ i ];
	}

	* (WORD *) &pHeader[ 67 ] = sum;

	return true;
}

int AMSDOSFileSystem::EnhanceFileData( NativeFile *pFile )
{
	pDir->ExtraReadDirectory( pFile );

	return 0;
}