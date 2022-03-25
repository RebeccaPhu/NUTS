#include "stdafx.h"

#include "SourceFunctions.h"

int ReplaceSourceContent( DataSource *pSource, CTempFile &FileObj )
{
	BYTE Buffer[ 131072 ];

	QWORD BytesToGo    = FileObj.Ext();
	QWORD SourceOffset = 0;

	FileObj.Seek( 0 );
	
	while ( BytesToGo > 0 )
	{
		QWORD BytesRead = BytesToGo;

		if ( BytesRead > 131072 )
		{
			BytesRead = 131072;
		}

		FileObj.Read( Buffer, (DWORD) BytesRead );

		pSource->WriteRaw( SourceOffset, (DWORD) BytesRead, Buffer );

		BytesToGo    -= BytesRead;
		SourceOffset += BytesRead;
	}

	pSource->PhysicalDiskSize = FileObj.Ext();

	pSource->Truncate( FileObj.Ext() );
	
	return 0;
}

int CopyContent( CTempFile &srcObj, CTempFile &destObj )
{
	BYTE Buffer[ 131072 ];

	QWORD BytesToGo    = srcObj.Ext();
	QWORD SourceOffset = 0;

	srcObj.Seek( 0 );
	
	while ( BytesToGo > 0 )
	{
		QWORD BytesRead = BytesToGo;

		if ( BytesRead > 131072 )
		{
			BytesRead = 131072;
		}

		srcObj.Read(   Buffer, (DWORD) BytesRead );
		destObj.Write( Buffer, (DWORD) BytesRead );

		BytesToGo    -= BytesRead;
		SourceOffset += BytesRead;
	}

	return 0;
}

int SourceToTemp( DataSource *pSource, CTempFile &destObj )
{
	BYTE Buffer[ 131072 ];

	QWORD BytesToGo    = pSource->PhysicalDiskSize;
	QWORD SourceOffset = 0;

	while ( BytesToGo > 0 )
	{
		QWORD BytesRead = BytesToGo;

		if ( BytesRead > 131072 )
		{
			BytesRead = 131072;
		}

		if ( pSource->ReadRaw( SourceOffset, (DWORD) BytesRead, Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		destObj.Write( Buffer, (DWORD) BytesRead );

		BytesToGo    -= BytesRead;
		SourceOffset += BytesRead;
	}

	return 0;
}