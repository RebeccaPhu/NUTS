#pragma once

#include "../NUTS/SCREENTranslator.h"

class SpriteTranslator : SCREENTranslator
{
public:
	SpriteTranslator(void);
	~SpriteTranslator(void);

	ModeList GetModes();

	int TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj );

	LogPalette GetLogicalPalette( DWORD ModeID );
	PhysPalette GetPhysicalPalette( void );
	PhysColours GetPhysicalColours( void );

	DWORD GuessMode( CTempFile &FileObj );

	AspectRatio GetAspect( void );

	DisplayPref GetDisplayPref( void )
	{
		return DisplayNatural;
	}

private:
	DWORD GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts );
};

