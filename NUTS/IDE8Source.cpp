#include "StdAfx.h"
#include "IDE8Source.h"
#include <malloc.h>

int	IDE8Source::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {
	unsigned char	*pWideBuffer	= (unsigned char *) malloc(SectorSize * 2);

	if (pSource->ReadSector(Sector, pWideBuffer, SectorSize * 2)) {
		free(pWideBuffer);

		return -1;
	}

	unsigned char *pSect	= (unsigned char *) pSectorBuf;

	for (int n=0; n<SectorSize; n++) { pSect[n] = pWideBuffer[n << 1]; }

	free(pWideBuffer);

	return 0;
}

int	IDE8Source::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
	unsigned char	*pWideBuffer	= (unsigned char *) malloc(SectorSize * 2);

	unsigned char *pSect	 = (unsigned char *) pSectorBuf;
	
	for (int n=0; n<SectorSize; n++) { pWideBuffer[n << 1] = pSect[n]; }

	if (pSource->WriteSector(Sector, pWideBuffer, SectorSize * 2)) {
		free(pWideBuffer);

		return -1;
	}

	free(pWideBuffer);

	return 0;
}
