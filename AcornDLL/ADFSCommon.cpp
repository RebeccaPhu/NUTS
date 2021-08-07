#include "StdAfx.h"
#include "ADFSCommon.h"

#include "Defs.h"
#include "../NUTS/libfuncs.h"
#include "../NUTS/NUTSError.h"

#include <time.h>

#include <sstream>
#include <iterator>

ADFSCommon::ADFSCommon(void)
{
	CommonUseResolvedIcons = false;

	CloneWars = false;

	Override  = false;
}


ADFSCommon::~ADFSCommon(void)
{
}


int ADFSCommon::ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
{
	BYTE tf[32];

	rstrncpy( tf, pFile->Filename, 16 );
	rstrncat( tf, (BYTE *) ".INF", 256 );

	sidecar.Filename = tf;

	BYTE INFData[80];

	rsprintf( INFData, "$.%s %06X %06X %s\n", pFile->Filename, pFile->LoadAddr & 0xFFFFFF, pFile->ExecAddr & 0xFFFFFF, ( pFile->AttrLocked ) ? "Locked" : "" );

	CTempFile *pTemp = (CTempFile *) sidecar.FileObj;

	pTemp->Seek( 0 );
	pTemp->Write( INFData, rstrnlen( INFData, 80 ) );

	return 0;
}

int ADFSCommon::ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj )
{
	int r = 0;

	if ( obj == nullptr )
	{
		BYTE tf[ 32 ];
		rstrncpy( tf, pFile->Filename, 16 );
		rstrncat( tf, (BYTE *) ".INF", 256 );

		sidecar.Filename = tf;
	}
	else
	{
		BYTE bINFData[ 256 ];

		ZeroMemory( bINFData, 256 );

		obj->Seek( 0 );
		obj->Read( bINFData, (DWORD) min( 256, obj->Ext() ) );

		std::string INFData( (char *) bINFData );

		std::istringstream iINFData( INFData );

		std::vector<std::string> parts((std::istream_iterator<std::string>(iINFData)), std::istream_iterator<std::string>());

		OverrideFile = *pFile;

		/* Need at least 3 parts */
		if ( parts.size() < 3 )
		{
			return 0;
		}

		try
		{
			/* Part deux is the load address. We'll use std::stoull */
			OverrideFile.LoadAddr = std::stoul( parts[ 1 ], nullptr, 16 );
			OverrideFile.LoadAddr &= 0xFFFFFF;

			/* Part the third is the exec address. Same again. */
			OverrideFile.ExecAddr = std::stoul( parts[ 2 ], nullptr, 16 );
			OverrideFile.ExecAddr &= 0xFFFFFF;
		}

		catch ( std::exception &e )
		{
			/* eh-oh */
			return NUTSError( 0x2E, std::wstring( L"Bad sidecar file: " ) + std::wstring( UString( (char *) e.what() ) ) );
		}

		/* Fix the standard attrs */
		OverrideFile.AttrRead   = 0xFFFFFFFF;
		OverrideFile.AttrWrite  = 0xFFFFFFFF;
		OverrideFile.AttrLocked = 0x00000000;

		/* Look through the remaining parts for "L" or "Locked" */
		for ( int i=3; i<parts.size(); i++ )
		{
			if ( ( parts[ i ] == "L" ) || ( parts[ i ] == "Locked" ) )
			{
				OverrideFile.AttrLocked = 0xFFFFFFFF;
			}
		}

		OverrideFile.Flags = 0;
		
		/*  Copy the filename - but we must remove the lading directory prefix*/
		if ( parts[ 0 ].substr( 1, 1 ) == "." )
		{
			OverrideFile.Filename = (BYTE *) parts[ 0 ].substr( 2 ).c_str();
		}
		else
		{
			OverrideFile.Filename = (BYTE *) parts[ 0 ].c_str();
		}

		Override = true;
	}

	return r;
}

void ADFSCommon::FreeAppIcons( Directory *pDirectory )
{
	ResolvedIcon_iter iIcon;

	for ( iIcon = pDirectory->ResolvedIcons.begin(); iIcon != pDirectory->ResolvedIcons.end(); iIcon++ )
	{
		if ( iIcon->second.pImage != nullptr )
		{
			free( iIcon->second.pImage );
		}
	}

	pDirectory->ResolvedIcons.clear();
}

void ADFSCommon::InterpretImportedType( NativeFile *pFile )
{
	if ( ( pFile->LoadAddr & 0xFFF00000 ) != 0xFFF00000 )
	{
		return;
	}

	DWORD FT;

	switch ( pFile->Type )
	{
	case FT_Arbitrary:
		FT = 0x000;
		break;

	case FT_Script:
		FT = 0xFEB;
		break;

	case FT_Spool:
		FT = 0xFFE;
		break;

	case FT_Text:
		FT = 0xFFF;
		break;

	case FT_BASIC:
		FT = 0xFFB;
		break;

	case FT_Binary:
	case FT_Code:
	case FT_Data:
	case FT_Graphic:
	case FT_Sound:
	case FT_Pref:
		FT = 0xFFD;
		break;

	case FT_DiskImage:
	case FT_TapeImage:
	case FT_MiscImage:
		FT = 0xFCE;
		break;

	case FT_None:
		FT = 0x000;
		break;

	}

	pFile->LoadAddr  = ( pFile->LoadAddr & 0x000000FF ) | ( FT << 8 );
	pFile->LoadAddr |= 0xFFF00000;
}

void ADFSCommon::InterpretNativeType( NativeFile *pFile )
{
	if ( ( pFile->LoadAddr & 0xFFF00000 ) != 0xFFF00000 )
	{
		return;
	}

	DWORD FT = pFile->RISCTYPE;

	pFile->LoadAddr  = ( pFile->LoadAddr & 0x000000FF ) | ( FT << 8 );
	pFile->LoadAddr |= 0xFFF00000;
}

void ADFSCommon::SetTimeStamp( NativeFile *pFile )
{
	if ( ( pFile->LoadAddr & 0xFFF00000 ) != 0xFFF00000 )
	{
		return;
	}

	/* This timestamp converstion is a little esoteric. RISC OS uses centiseconds
	   since 0:0:0@1/1/1900. This is *nearly* unixtime. */

	time_t UnixTime = time( NULL );

	QWORD FileTime  = UnixTime;
	      FileTime += 2208988800; // Secs difference between 1/1/1970 and 1/1/1900.
	      FileTime *= 100;

	pFile->LoadAddr = ( pFile->LoadAddr & 0xFFFFFF00 ) | (DWORD) ( FileTime & 0xFF );
	pFile->ExecAddr = (DWORD) ( ( FileTime & 0xFFFFFFFF00 ) >> 8) ;
}

int ADFSCommon::RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory, bool AllowLongNames )
{
	/* The idea here is to try and alter the name of the directory to something unique.

	   We're going to do this by copying the filename, then overwriting the trail with
	   a number starting at 1, and incrementing on each pass. The number is prefixed by _

	   When we find one, we'll return it. If not, we go around again. If we somehow fill
	   up all 10 characters, we'll error here.
	*/

	bool Done = false;

	BYTEString CurrentFilename( pDir->Filename, pDir->Filename.length() + 11 );
	BYTEString ProposedFilename( pDir->Filename.length() + 11 );

	size_t InitialLen = CurrentFilename.length();

	DWORD ProposedIndex = 1;

	BYTE ProposedSuffix[ 11 ];

	while ( !Done )
	{
		rstrncpy( ProposedFilename, CurrentFilename, InitialLen );

		rsprintf( ProposedSuffix, "_%d", ProposedIndex );

		WORD SuffixLen = rstrnlen( ProposedSuffix, 9 );
		
		/* If we allow long file names, then we just add it on the end, otherwise we have the 10 char limit */
		if ( ( AllowLongNames ) || ( ( InitialLen + SuffixLen ) <= 10 ) )
		{
			rstrcpy( &ProposedFilename[ InitialLen ], ProposedSuffix );
		}
		else
		{
			rstrncpy( &ProposedFilename[ 10 - SuffixLen ], ProposedSuffix, 10 );
		}

		/* Now see if that's unique */
		NativeFileIterator iFile;

		bool Unique = true;

		for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
		{
			if ( rstricmp( iFile->Filename, ProposedFilename ) )
			{
				Unique = false;

				break;
			}
		}

		Done = Unique;

		ProposedIndex++;

		if ( ProposedIndex == 0 )
		{
			/* EEP - Ran out */
			return NUTSError( 208, L"Unable to find unique name for incoming directory" );
		}
	}

	pDir->Filename = ProposedFilename;

	return 0;
}