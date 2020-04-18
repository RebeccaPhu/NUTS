#pragma once

#include "../NUTS/SCREENTranslator.h"
#include "../NUTS/TempFile.h"

class AcornSCREENTranslator :
	public SCREENTranslator
{
public:
	AcornSCREENTranslator(void);
	~AcornSCREENTranslator(void);

	ModeList GetModes();

	int TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj );
	int DoTeletextTranslation( GFXTranslateOptions &Options, CTempFile &FileObj );

	LogPalette GetLogicalPalette( DWORD ModeID );
	PhysPalette GetPhysicalPalette( void );
	PhysColours GetPhysicalColours( void );

	DWORD GuessMode( CTempFile &FileObj );

	AspectRatio GetAspect( void );

	BYTE Mode;

public:
	static BYTE *pTeletextFont;

private:
	DWORD GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts );
};

