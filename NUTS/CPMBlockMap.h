#pragma once

#include "DataSource.h"

class CPMBlockMap
{
public:
	CPMBlockMap( DataSource *pDataSource )
	{
		pSource = pDataSource;

		DS_RETAIN( pSource );

		pBlocks   = nullptr;
		NumBlocks = 0;
	}

	~CPMBlockMap(void)
	{
		DS_RELEASE( pSource );

		if ( pBlocks != nullptr )
		{
			free( pBlocks );
		}
	}

public:
	void InitialiseMap( DWORD Blocks )
	{
		pBlocks   = (BYTE *) realloc( pBlocks, Blocks );
		NumBlocks = Blocks;

		ClearMap();
	}

	void ClearMap( void )
	{
		ZeroMemory( pBlocks, NumBlocks );
	}

	void SetBlock( DWORD Block )
	{
		if ( Block >= NumBlocks )
		{
			return;
		}
		
		pBlocks[ Block ] = 0xFF;
	}

	void ClearBlock( DWORD Block )
	{
		if ( Block >= NumBlocks )
		{
			return;
		}
		
		pBlocks[ Block ] = 0x00;
	}

	bool IsFree( DWORD Block )
	{
		if ( Block >= NumBlocks )
		{
			return false;
		}
		
		return ( pBlocks[ Block ] == 0xFF ) ? true : false;
	}

	DWORD GetFreeBlock( void ) {
		for ( DWORD b=0; b<NumBlocks; b++ )
		{
			if ( pBlocks[ b ] == 0 )
			{
				return b;
			}
		}

		return 0xFFFFFFFF;
	}

	DWORD FreeBlocks( void )
	{
		DWORD Count = 0;

		for ( DWORD b=0; b<NumBlocks; b++ )
		{
			if ( pBlocks[ b ] == 0 )
			{
				Count++;
			}
		}

		return Count;
	}

private:
	DataSource *pSource;

	BYTE  *pBlocks;
	DWORD NumBlocks;
};

