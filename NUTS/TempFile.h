#pragma once

#include <string>

#include "Defs.h"

class CTempFile
{
public:
	CTempFile(void);
	CTempFile( std::string exFile );
	~CTempFile(void);

private:
	std::string PathName;
	QWORD Ptr;

	bool bKeep;

public:
	void Seek( QWORD NewPtr );
	QWORD Ext( void );
	void Write( void *Buffer, DWORD Length );
	void Read( void *Buffer, DWORD Length );
	std::string Name( void ) { return PathName; }
	void SetExt( QWORD NewPtr );
	void Keep( void );
};

