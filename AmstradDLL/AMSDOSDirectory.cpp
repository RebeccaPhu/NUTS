#include "StdAfx.h"

#include "AMSDOSDirectory.h"
#include "Defs.h"
#include "../NUTS/libfuncs.h"

void AMSDOSDirectory::ExtraReadDirectory( NativeFile *pFile )
{
	if ( pFile->Flags & FF_Extension )
	{
		if ( rstrnicmp( pFile->Extension, (BYTE *) "BAS", 3 ) )
		{
			pFile->Type = FT_BASIC;
			pFile->Icon = FT_BASIC;

			pFile->XlatorID = TUID_LOCO;
		}
	}
}