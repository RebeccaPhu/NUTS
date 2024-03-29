#include "StdAfx.h"
#include "IDE8Source.h"
#include <malloc.h>

int	IDE8Source::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	BYTE *pWideBuffer = (BYTE *) malloc(SectorSize * 2);

	if ( pSource->ReadSectorLBA(Sector, pWideBuffer, SectorSize * 2) != DS_SUCCESS ) {
		free(pWideBuffer);

		return -1;
	}

	BYTE *pSect = (BYTE *) pSectorBuf;

	for (DWORD n=0; n<SectorSize; n++) { pSect[n] = pWideBuffer[n << 1]; }

	free(pWideBuffer);

	return 0;
}

int	IDE8Source::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	BYTE *pWideBuffer = (BYTE *) malloc(SectorSize * 2);
	BYTE *pSect       = (BYTE *) pSectorBuf;
	
	for (DWORD n=0; n<SectorSize; n++) { pWideBuffer[n << 1] = pSect[n]; }

	if ( pSource->WriteSectorLBA(Sector, pWideBuffer, SectorSize * 2) != DS_SUCCESS ) {
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

	if ( r == DS_SUCCESS )
	{
		for (DWORD n=0; n<Length; n++)
		{
			pBuffer[n] = pWideBuffer[n << 1];
		}
	}

	free(pWideBuffer);

	return r;
}

int IDE8Source::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	BYTE *pWideBuffer = (BYTE *) malloc(Length * 2);
	BYTE *pRaw        = (BYTE *) pBuffer;

	for (DWORD n=0; n<Length; n++)
	{
		pWideBuffer[n << 1] = pBuffer[n];
	}

	int r = pSource->WriteRaw( Offset << 1, Length << 1, pWideBuffer );

	free(pWideBuffer);

	return r;
}
