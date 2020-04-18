#pragma once

#include "../NUTS/SCREENTranslator.h"

class SpectrumSCREENTranslator :
	public SCREENTranslator
{
public:
	SpectrumSCREENTranslator(void);
	~SpectrumSCREENTranslator(void);

public:
	ModeList GetModes()
	{
		ModeList None;

		return None;
	}

	int TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj );

	LogPalette  GetLogicalPalette( DWORD ModeID )
	{
		LogPalette None;

		return None;
	}

	PhysPalette GetPhysicalPalette( void );
	PhysColours GetPhysicalColours( void );

	DWORD GuessMode( CTempFile &FileObj )
	{
		return 0;
	}

	AspectRatio GetAspect( void )
	{
		return AspectRatio( 4, 3 );
	}

	DWORD GetColour( WORD ColIndex, GFXTranslateOptions *opts )
	{
		DWORD pix = 0x00777777;

		if ( opts->pPhysicalPalette->find( ColIndex ) != opts->pPhysicalPalette->end() )
		{
			pix = opts->pPhysicalPalette->at( ColIndex );
		}

		return ( ( pix & 0xFF0000 ) >> 16 ) | ( pix & 0xFF00 ) | ( ( pix & 0xFF ) << 16 );
	}
};

