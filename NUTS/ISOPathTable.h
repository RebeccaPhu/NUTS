#pragma once

#include "Defs.h"
#include "DataSource.h"

typedef struct _ISOPathTableEntry
{
	BYTEString DirectoryName;
	DWORD      Extent;
	WORD       ParentNum; // I hate you.
	WORD       DirectoryNum;
	DWORD      ParentExtent;
	bool       IsRoot;
} ISOPathTableEntry;

typedef std::vector<ISOPathTableEntry> ISOPathTableEntries;

typedef ISOPathTableEntries::iterator ISOPathTableIterator;

typedef std::vector<ISOPathTableEntries> ISOPathTree;

typedef ISOPathTree::iterator ISOPathTreeIterator;

class ISOPathTable
{
public:
	ISOPathTable( DataSource *pSrc, DWORD dwSectorSize );
	~ISOPathTable(void);

public:
	void  ReadPathTable( DWORD Extent, DWORD Size, DWORD FSID );
	void  WritePathTable( DWORD Extent, bool IsMTable, DWORD FSID );
	DWORD GetProjectedSize();
	void  AddDirectory( BYTEString &Identifier, DWORD Extent, DWORD ParentExt );
	void  RemoveDirectory( DWORD Extent );
	void  BlankTable( DWORD RootExtent );
	DWORD GetNumEntries();
	DWORD ExtentForDir( BYTEString &Identifier, DWORD ParentExtent );

private:
	ISOPathTableEntries TableEntries;
	ISOPathTree         PathTree;

	DataSource *pSource;
	DWORD      SectorSize;

	DWORD      RootExtent;

private:
	void  CompilePathTree();
	void  CompilePathTable();
};

