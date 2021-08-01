#ifndef _BYTESTRING_H
#define _BYTESTRING_H

#include <malloc.h>

class BYTEString
{
public:
	BYTEString()
	{
		bytes = 0;
		pMem  = empty;

		empty[0] = 0;
	}

	BYTEString( const size_t sz )
	{
		bytes = sz;

		pMem  = ( BYTE * ) malloc( sz );

		memset( pMem, 0, sz );

		empty[0] = 0;
	}

	BYTEString( BYTE *pSource )
	{
		bytes = 0;
		BYTE *p = pSource;

		while ( *p != 0 ) { bytes++; p++; }

		bytes++;

		pMem = (BYTE *) malloc( bytes );

		memcpy( pMem, pSource, bytes );

		empty[0] = 0;
	}

	BYTEString( const BYTE *pSource, const size_t len )
	{
		bytes = len + 1;

		pMem = (BYTE *) malloc( bytes );

		memcpy( pMem, pSource, len );

		pMem[ len ] = 0;

		empty[0] = 0;
	}

	BYTEString( const BYTEString &source )
	{
		pMem = (BYTE *) malloc( source.bytes );

		memcpy( pMem, source.pMem, source.bytes );

		bytes = source.bytes;

		empty[0] = 0;
	}

	~BYTEString()
	{
		if ( bytes != 0 )
		{
			free( (void *) pMem );
		}

		bytes = 0;
	}

	size_t size() const
	{
		return bytes;
	}

	size_t length() const 
	{
		if ( bytes == 0 ) { return 0; }

		return bytes - 1;
	}

	BYTEString &operator =( const BYTEString &source )
	{
		if ( bytes != 0 )
		{
			free( (void *) pMem );
		}

		pMem = (BYTE *) malloc( source.bytes );

		memcpy( pMem, source.pMem, source.bytes );

		bytes = source.bytes;

		return *this;
	}

	operator BYTE * const() { return ( (bytes==0)?empty:pMem ); }

	operator char * const() { return (char *) ( (bytes==0)?empty:pMem ); }

	BYTE & operator[](int i) { return pMem[ i ]; }

private:
	size_t bytes;
	BYTE *pMem;
	BYTE empty[1];

};

#endif