#include "StdAfx.h"
#include "DOS3FileSystem.h"

#include "../NUTS/CPM.h"
#include "DOS3FileSystem.h"
#include "DOS3Directory.h"
#include "SinclairDefs.h"

CPMDPB DOS3_DPB = {
	/* .ShortFSName  = */ "+3DOS",
	/* .Encoding     = */ ENCODING_SINCLAIR,
	/* .FSFileType   = */ FT_SINCLAIR_DOS,
	/* .SysTracks    = */ 1,
	/* .SecsPerTrack = */ 9,
	/* .SecSize      = */ 0x200,
	/* .FSID         = */ FSID_DOS3,
	/* .DirSecs      = */ 4,
	/* .ExtentSize   = */ 1024,
	/* .Flags        = */ CF_UseHeaders | CF_Force8bit,
	/* .HeaderSize   = */ 0x80,
};

DOS3FileSystem::DOS3FileSystem(DataSource *pDataSource) : CPMFileSystem( pDataSource, DOS3_DPB )
{
	/* Remove the directory object and create a new one */
	delete pDir;

	DOS3_DPB.Encoding = ENCODING_SINCLAIR;
	dpb.Encoding      = ENCODING_SINCLAIR;

	pDir = (CPMDirectory *) new DOS3Directory( pDataSource, pMap, DOS3_DPB );

	pDirectory = (Directory *) pDir;

	Flags =
		FSF_Creates_Image | FSF_Formats_Image |
		FSF_SupportBlocks | FSF_SupportFreeSpace | FSF_Capacity |
		FSF_FixedSize | FSF_UseSectors | FSF_Uses_DSK | FSF_No_Quick_Format | FSF_Uses_Extensions;
}

bool DOS3FileSystem::IncludeHeader( BYTE *pHeader )
{
	if ( !rstrnicmp( pHeader, (BYTE *) "PLUS3DOS", 8 ) )
	{
		return true;
	}

	WORD sum = 0;

	for ( BYTE i=0; i<0x7F; i++ )
	{
		sum += pHeader[ i ];
	}

	sum &= 0xFF;

	if ( pHeader[ 0x7F ] == (BYTE) sum )
	{
		return false;
	}

	return true;
}

FSHint DOS3FileSystem::Offer( BYTE *Extension )
{
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

	/* +3DOS Disk */
	if ( ( set.size() > 0 ) && ( set[ 0 ] >= 0x01 ) && ( set[ 0 ] <= 0x09 ) )
	{
		hint.Confidence += 10;
	}

	return hint;
}

std::vector<AttrDesc> DOS3FileSystem::GetAttributeDescriptions( void )
{
	std::vector<AttrDesc> Attrs = CPMFileSystem::GetAttributeDescriptions();

	AttrDesc Attr;

	/* Load address Hex, visible, disabled */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

    /* File Type. */
	Attr.Index = 6;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrFile;
	Attr.Name  = L"File Type";

	static std::wstring Names[ 4 ] = {
		L"Program", L"Number Array", L"Character Array", L"Bytes"
	};

	for ( BYTE i=0; i<4; i++ )
	{
		AttrOption opt;

		opt.Name            = Names[ i ];
		opt.EquivalentValue = i;
		opt.Dangerous       = true;

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	/* Auto start line */
	Attr.Index = 7;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrDec | AttrWarning | AttrFile;
	Attr.Name  = L"Auto start line";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 8;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Vars Offset";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 9;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrDanger | AttrFile;;
	Attr.Name  = L"Variable ID";
	Attrs.push_back( Attr );

	Attr.Index = 10 ;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDanger;
	Attr.Name  = L"Has header";

	Attrs.push_back( Attr );

	return Attrs;
}

bool DOS3FileSystem::GetCPMHeader( NativeFile *pFile, BYTE *pHeader )
{
	if ( pFile->Attributes[ 10 ] )
	{
		ZeroMemory( pHeader, 0x80 );

		rsprintf( pHeader, "PLUS3DOS" );

		pHeader[ 8  ] = 0x1A;
		pHeader[ 9  ] = 0x01;
		pHeader[ 10 ] = 0x00;

		* (DWORD *) &pHeader[ 11 ] = (DWORD) pFile->Length;

		/* +3 BASIC header goes here*/
		pHeader[ 15 ] = max( pFile->Attributes[ 6 ], 3 );

		* (WORD *) &pHeader[ 16 ] = (WORD) pFile->Length;

		switch ( pFile->Attributes[ 6 ] )
		{
		case 0x00:
			* (WORD *) &pHeader[ 18 ] = (WORD) pFile->Attributes[ 7 ];
			* (WORD *) &pHeader[ 20 ] = (WORD) pFile->Attributes[ 8 ];
			break;

		case 0x01:
		case 0x02:
			pHeader[ 19 ] = pFile->Attributes[ 9 ];
			break;

		default:
			* (WORD *) &pHeader[ 18 ] = (WORD) pFile->Attributes[ 5 ];
			break;
		}

		pHeader[ 22 ] = 0;

		WORD sum = 0;

		for ( BYTE i=0; i<127; i++ )
		{
			sum += pHeader[ i ];
		}

		pHeader[ 127 ] = (BYTE) ( sum & 0xFF );

		return true;
	}

	return false;
}

int DOS3FileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	for ( BYTE i=0; i<10; i++ )
	{
		pFile->Attributes[ i ] = Changes->Attributes[ i ];
	}

	if ( pFile->Attributes[ 10 ] == Changes->Attributes[ 10 ] )
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

	pFile->Attributes[ 10 ] = Changes->Attributes[ 10 ];

	NativeFile file = *pFile;

	/* Read, delete and rewrite so that the header data changes */
	CTempFile tmp;

	ReadFile( FileID, tmp );
	DeleteFile( FileID );
	WriteFile( &file, tmp );

	return 0;
}

int DOS3FileSystem::Format_Process( FormatType FT, HWND hWnd )
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

	/* Low-level format */
	for ( BYTE t=0; t<40; t++ )
	{
		pSource->SeekTrack( t );

		TrackDefinition tr;

		tr.Density = DoubleDensity;
		tr.GAP1.Repeats = 80;
		tr.GAP1.Value   = 0x4E;

		for ( BYTE s=1; s<10; s++ )
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
			sd.SectorID = s;

			sd.GAP3.Repeats = 22;
			sd.GAP3.Value   = 0x4E;

			sd.GAP3PLL.Repeats = 12;
			sd.GAP3PLL.Value   = 0;
			
			sd.GAP3SYNC.Repeats = 3;
			sd.GAP3SYNC.Value   = 0xA1;

			sd.DAM = 0xFB;

			memset( sd.Data, 0xE5, 512 );

			sd.GAP4.Repeats     = 40;
			sd.GAP4.Value       = 0x4E;

			tr.Sectors.push_back( sd );

			swprintf_s( FormatMsg, 256, L"Formatting track %d sector %d", t, s );

			SendMessage( hWnd, WM_FORMATPROGRESS, Percent( t, 80, s, 9, false ), (LPARAM) FormatMsg );
		}
		
		tr.GAP5 = 0x4E;

		pSource->WriteTrack( tr );
	}

	pSource->EndFormat();

	SendMessage( hWnd, WM_FORMATPROGRESS, 100, (LPARAM) pDone );

	return 0;
}

void DOS3FileSystem::CPMPreWriteCheck( NativeFile *pFile )
{
	if ( pFile->FSFileType == FT_SINCLAIR )
	{
		/* Sinclair tape file. Convert to headered +3DOS fle */
		NativeFile file = *pFile;

		pFile->Attributes[ 5 ] = file.Attributes[ 1 ];
		pFile->Attributes[ 6 ] = file.Attributes[ 2 ];
		pFile->Attributes[ 7 ] = file.Attributes[ 3 ];
		pFile->Attributes[ 8 ] = file.Attributes[ 4 ];
		pFile->Attributes[ 9 ] = file.Attributes[ 5 ];

		pFile->Attributes[ 10 ] = 0xFFFFFFFF;

		switch ( pFile->Attributes[ 2 ] )
		{
		case 0:
			rsprintf( pFile->Extension, (BYTE *) "BAS" );
			break;

		case 1:
		case 2:
			rsprintf( pFile->Extension, (BYTE *) "DAT" );
			break;

		default:
			rsprintf( pFile->Extension, (BYTE *) "BIN" );
			break;
		}

		pFile->Flags |= FF_Extension;

		pFile->FSFileType = FT_SINCLAIR_DOS;
	}
}

int DOS3FileSystem::EnhanceFileData( NativeFile *pFile )
{
	pDir->ExtraReadDirectory( pFile );

	return 0;
}
