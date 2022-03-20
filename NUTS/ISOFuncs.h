#ifndef ISOFUNCS_H
#define ISOFUNCS_H

#include "ISODefs.h"
#include "DataSource.h"
#include "NativeFile.h"

void ISOStrTerm( BYTE *p, WORD l );
void ISOStrStore( BYTE *p, BYTE *s, WORD ml );
void JolietStrTerm( BYTE *jp, WORD jl );
void ISOJolietStore( BYTE *p, BYTE *s, WORD bl );

DWORD RemapSector( DWORD OldSector, ISOSectorList &Sectors, BYTE IsoOp );

int  ISORestructure( DataSource *pSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD SectorSize, FSIdentifier FSID );

int  PerformISOJob( ISOJob *pJob );

int  ISOExtendDataArea( DataSource *pSource, DWORD SectorSize, DWORD Sectors, FSIdentifier FSID );
int  ISOSetPathTableSize( DataSource *pSource, DWORD SectorSize, DWORD TableSize, FSIdentifier FSID );
int  ISOSetRootSize( DataSource *pSource, DWORD SectorSize, DWORD RootSize, FSIdentifier FSID );
int  ISOSetDirSize( DataSource *pSource, DWORD SectorSize, DWORD RootSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, FSIdentifier FSID );
int  ISOSetSubDirParentSize( DataSource *pSource, DWORD SectorSize, DWORD DirSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, FSIdentifier FSID );

void ScanRealImageSize( DataSource *pSource, ISOVolDesc *pVolDesc, FSIdentifier FSID );

DWORD ISOChooseCapacity();

void PrepareISOSectors( ISOSectorList &Sectors );

void MakeISOFilename( NativeFile *pFile, BYTE *StringBuf, bool IncludeRevision = true );

void SetISOHints( DataSource *pSource, bool IsEOF, bool IsEOR );

#endif