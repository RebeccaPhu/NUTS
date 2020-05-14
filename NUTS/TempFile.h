#pragma once

#include <string>

#include "Defs.h"

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

public:
	void Seek( QWORD NewPtr );
	QWORD Ext( void );
	void Write( void *Buffer, DWORD Length );
	void Read( void *Buffer, DWORD Length );
	std::wstring Name( void ) { return PathName; }
	void SetExt( QWORD NewPtr );
	void Keep( void );
};

