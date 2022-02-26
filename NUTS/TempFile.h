#pragma once

#include <string>

#include "Defs.h"

#define MAX_MEMORY_SIZE 4*1048576

class CTempFile
{
public:
	CTempFile(void);
	CTempFile( std::wstring exFile );
	~CTempFile(void);

private:
	std::wstring PathName;
	QWORD Ptr;

	bool bKeep;

	bool InMemory;

	BYTE *pMemory;
	DWORD MemorySize;

public:
	void Seek( QWORD NewPtr );
	QWORD Ext( void );
	void Write( void *Buffer, DWORD Length );
	void Read( void *Buffer, DWORD Length );
	std::wstring Name( void ) { return PathName; }
	void SetExt( QWORD NewPtr );
	void Keep( void );
	void KeepAs( std::wstring Filename );
	void Dump();

private:
	void DumpMemory();
};

