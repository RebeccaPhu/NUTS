#pragma once

#include "../nuts/Directory.h"

typedef struct _TZXPB {
	FSIdentifier FSID;
	std::wstring LocalMenu;
	BYTE  FSName[ 32 ];
	EncodingIdentifier Encoding;
	FTIdentifier FT;
	BYTE  Extension[ 4 ];
	std::wstring RegistryPrefix;
} TZXPB;

#define FT_TZX L"Generic_TZX_Object"

class TZXDirectory :
	public Directory
{
public:
	TZXDirectory( DataSource *pDataSource, TZXPB &pb ) : Directory( pDataSource )
	{
		tzxpb = pb;
	}

	~TZXDirectory(void)
	{
	}

	virtual int ReadDirectory( void );
	virtual int WriteDirectory( void ) { return 0; }

	virtual void ResolveFiles( void ) { return; }

	const BYTE *IdentifyBlock( BYTE BlockID );

private:
	QWORD ReadOffset;

	TZXPB tzxpb;

private:
	void ParseBlock( BYTE *BlockID, WORD *BlockSize );

	inline BYTE GetByte()
	{
		BYTE x;

		pSource->ReadRaw( ReadOffset, 1, &x );

		ReadOffset++;

		return x;
	}

	inline WORD GetWord()
	{
		WORD w;

		pSource->ReadRaw( ReadOffset, 2, (BYTE *) &w );

		ReadOffset+=2;

		return w;
	}		

	inline DWORD GetTriByte()
	{
		DWORD w = 0;

		pSource->ReadRaw( ReadOffset, 3, (BYTE *) &w );

		ReadOffset+=3;

		return w;
	}		

	inline DWORD GetDWord()
	{
		DWORD w;

		pSource->ReadRaw( ReadOffset, 4, (BYTE *) &w );

		ReadOffset+=4;

		return w;
	}		

public:
	std::vector<DWORD> BlockLengths;
};

