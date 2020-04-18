#pragma once

#include <vector>
#include <map>
#include <utility>
#include "TempFile.h"
#include "Defs.h"

class SCREENTranslator
{
public:
	SCREENTranslator(void) {
	}
	~SCREENTranslator(void) {
	}

	virtual ModeList GetModes()
	{
		ModeList Modes;

		return Modes;
		}

	virtual int TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj )
	{
		return -1;
	}

	virtual LogPalette GetLogicalPalette( DWORD ModeID )
	{
		LogPalette LogicalPalette;

		return LogicalPalette;
	}

	virtual PhysPalette GetPhysicalPalette( void )
	{
		PhysPalette PhysicalPalette;

		return PhysicalPalette;
	}

	virtual PhysColours GetPhysicalColours( void )
	{
		PhysColours PhysicalColours;

		return PhysicalColours;
	}

	virtual DWORD GetColour( WORD ColIndex, GFXTranslateOptions *opts )
	{
		WORD PhysicalIndex = opts->pLogicalPalette->at( ColIndex );

		std::pair<WORD,WORD> PhysicalColour = opts->pPhysicalColours->at( PhysicalIndex );

		DWORD pix;

		if ( opts->FlashPhase )
		{
			pix = opts->pPhysicalPalette->at( PhysicalColour.second );
		}
		else
		{
			pix = opts->pPhysicalPalette->at( PhysicalColour.first );
		}

		return ( ( pix & 0xFF0000 ) >> 16 ) | ( pix & 0xFF00 ) | ( ( pix & 0xFF ) << 16 );
	}

	virtual DWORD GuessMode( CTempFile &FileObj )
	{
		return 0;
	}

	virtual AspectRatio GetAspect( void )
	{
		return std::make_pair<BYTE,BYTE>( 4, 3 );
	}

	virtual DisplayPref GetDisplayPref( void )
	{
		return DisplayScaledScreen;
	}
};

