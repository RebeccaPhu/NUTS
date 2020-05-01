#include "StdAfx.h"
#include "IDE8Source.h"
#include <malloc.h>

int	IDE8Source::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {
	BYTE *pWideBuffer = (BYTE *) malloc(SectorSize * 2);

	if (pSource->ReadSector(Sector, pWideBuffer, SectorSize * 2)) {
		free(pWideBuffer);

		return -1;
	}

	BYTE *pSect = (BYTE *) pSectorBuf;

	for (int n=0; n<SectorSize; n++) { pSect[n] = pWideBuffer[n << 1]; }

	free(pWideBuffer);

	return 0;
}

int	IDE8Source::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
	BYTE *pWideBuffer = (BYTE *) malloc(SectorSize * 2);
	BYTE *pSect       = (BYTE *) pSectorBuf;
	
	for (int n=0; n<SectorSize; n++) { pWideBuffer[n << 1] = pSect[n]; }

	if (pSource->WriteSector(Sector, pWideBuffer, SectorSize * 2)) {
		free(pWideBuffer);

		return -1;
	}

	free(pWideBuffer);

	return 0;
}

int IDE8Source::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	BYTE *pWideBuffer = (BYTE *) malloc(Length * 2);
	BYTE *pRaw        = (BYTE *) pBuffer;

	int r = pSource->ReadRaw( Offset << 1, Length << 1, pWideBuffer );

	if ( !r )
	{
		for (int n=0; n<Length; n++)
		{
			pBuffer[n] = pWideBuffer[n << 1];
		}
	}

	free(pWideBuffer);

	return r;
}
