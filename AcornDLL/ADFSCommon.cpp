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
}


ADFSCommon::~ADFSCommon(void)
{
}


int ADFSCommon::ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
{
	rstrncpy( sidecar.Filename, pFile->Filename, 16 );
	rstrncat( sidecar.Filename, (BYTE *) ".INF", 256 );

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
		rstrncpy( sidecar.Filename, pFile->Filename, 16 );
		rstrncat( sidecar.Filename, (BYTE *) ".INF", 256 );
	}
	else
	{
		BYTE bINFData[ 256 ];

		ZeroMemory( bINFData, 256 );

		obj->Seek( 0 );
		obj->Read( bINFData, min( 256, obj->Ext() ) );

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
			return NUTSError( 0x2E, L"Bad sidecar file" );
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
			rstrncpy( OverrideFile.Filename, (BYTE *) parts[ 0 ].substr( 2 ).c_str(), 10 );
		}
		else
		{
			rstrncpy( OverrideFile.Filename, (BYTE *) parts[ 0 ].c_str(), 10 );
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
	if ( pFile->LoadAddr & 0xFFF00000 != 0xFFF00000 )
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

void ADFSCommon::SetTimeStamp( NativeFile *pFile )
{
	if ( pFile->LoadAddr & 0xFFF00000 != 0xFFF00000 )
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