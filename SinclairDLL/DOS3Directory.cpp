#include "StdAfx.h"
#include "DOS3Directory.h"

#include "SinclairDefs.h"
#include "../NUTS/libfuncs.h"

void DOS3Directory::ExtraReadDirectory( NativeFile *pFile )
{
	if ( pFile->Flags & FF_Extension )
	{
		if ( rstrnicmp( pFile->Extension, (BYTE *) "BAS", 3 ) )
		{
			pFile->Type = FT_BASIC;
			pFile->Icon = FT_BASIC;

			pFile->XlatorID = BASIC_SPECTRUM;
		}
	}
}