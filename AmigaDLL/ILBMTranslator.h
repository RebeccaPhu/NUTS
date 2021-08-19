#pragma once

#include "../NUTS/SCREENTranslator.h"
#include "../NUTS/TempFile.h"
#include "Defs.h"

typedef struct _BMHD {
	WORD Width;
	WORD Height;
	BYTE Planes;
	BYTE Compression;
	BYTE AspectX;
	BYTE AspectY;
	bool Mask;

	QWORD DataOffset;

	bool Valid;
} BMHD;

class ILBMTranslator :
	public SCREENTranslator
{
public:
	ILBMTranslator(void);
	~ILBMTranslator(void);

	int TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj );

	PhysPalette GetPhysicalPalette( void );
	PhysColours GetPhysicalColours( void );

	AspectRatio GetAspect( void );

public:

private:
	DWORD GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts );

	void ReadIFFHeader( CTempFile *obj );
	void UnpackPlanar( BYTE *PlanarRow, DWORD *pRow, BYTE plane, DWORD Width );

	PhysColours PColours;
	PhysPalette Palette;

private:
	BMHD ImageHeader;
};
