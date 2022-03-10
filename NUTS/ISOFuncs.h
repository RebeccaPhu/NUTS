#ifndef ISOFUNCS_H
#define ISOFUNCS_H

#include "Defs.h"
#include "ISODefs.h"
#include "DataSource.h"

void ISOStrTerm( BYTE *p, WORD l );
void ISOStrStore( BYTE *p, BYTE *s, WORD ml );
void JolietStrTerm( BYTE *jp, WORD jl );
void ISOJolietStore( BYTE *p, BYTE *s, WORD bl );

DWORD RemapSector( DWORD OldSector, ISOSectorList &Sectors, BYTE IsoOp );

int  ISORestructure( DataSource *pSource, ISOSectorList &Sectors, BYTE IsoOp, DWORD SectorSize, DWORD FSID );

int  PerformISOJob( ISOJob *pJob );

int  ISOExtendDataArea( DataSource *pSource, DWORD SectorSize, DWORD Sectors, DWORD FSID );
int  ISOSetPathTableSize( DataSource *pSource, DWORD SectorSize, DWORD TableSize, DWORD FSID );
int  ISOSetRootSize( DataSource *pSource, DWORD SectorSize, DWORD RootSize, DWORD FSID );
int  ISOSetDirSize( DataSource *pSource, DWORD SectorSize, DWORD RootSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, DWORD FSID );
int  ISOSetSubDirParentSize( DataSource *pSource, DWORD SectorSize, DWORD DirSize, DWORD ParentSector, DWORD ParentLength, DWORD DirSector, DWORD FSID );

void ScanRealImageSize( DataSource *pSource, ISOVolDesc *pVolDesc, DWORD FSID );

DWORD ISOChooseCapacity();

void PrepareISOSectors( ISOSectorList &Sectors );

void MakeISOFilename( NativeFile *pFile, BYTE *StringBuf, bool IncludeRevision = true );

void SetISOHints( DataSource *pSource, bool IsEOF, bool IsEOR );

#endif