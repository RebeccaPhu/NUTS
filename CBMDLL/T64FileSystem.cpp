#include "StdAfx.h"
#include "T64FileSystem.h"

#include "CBMDefs.h"
#include "CBMFunctions.h"
#include "../NUTS/libfuncs.h"
#include "../NUTS/SourceFunctions.h"
#include "../NUTS/TempFile.h"

FSHint T64FileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.FSID       = FSID_T64;
	hint.Confidence = 0;

	if ( Extension != nullptr )
	{
		if ( rstrnicmp( Extension, (BYTE *) "T64", 3 ) )
		{
			hint.Confidence += 15;
		}
	}

	BYTE Buf[ 32 ];

	pSource->ReadRaw( 0, 32, Buf );

	if ( rstrncmp( Buf, (BYTE *) "C64 tape image file", 19 ) )
	{
		hint.Confidence += 10;
	}
	else if ( rstrncmp( Buf, (BYTE *) "C64S tape file", 14 ) )
	{
		hint.Confidence += 10;
	}
	else if ( rstrncmp( Buf, (BYTE *) "C64", 3 ) )
	{
		hint.Confidence += 5;
	}

	return hint;
}

int T64FileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE Buffer[ 16384 ];

	DWORD BytesToGo = (DWORD) pFile->Length - 2;
	QWORD Offset    = pFile->Attributes[ 1 ];
	WORD  Adx       = (WORD) pFile->Attributes[ 2 ];

	store.Write( &Adx, 2 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( BytesToGo, 16384 );

		if ( pSource->ReadRaw( Offset, BytesRead, Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		store.Write( Buffer, BytesRead );

		BytesToGo -= BytesRead;
		Offset    += BytesRead;
	}

	return 0;
}

int T64FileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	/* No existence check as this is a tape image, but we will do an encoding check */
	NativeFile file = *pFile;

	if ( pFile->EncodingID != ENCODING_PETSCII )
	{
		if ( pFile->EncodingID != ENCODING_ASCII )
		{
			return FILEOP_NEEDS_ASCII;
		}
	}

	IncomingASCII( &file );

	return RegenerateSource( 0, 0, &file, &store, T64_REGEN_WRITE );
}


int T64FileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE Entry[ 32 ];

	if ( pSource->ReadRaw( pFile->Attributes[ 0 ], 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}

	rstrncpy( &Entry[ 0x10 ], NewName, 16 );

	TFromString( &Entry[ 0x10 ], 16 );

	if ( pSource->WriteRaw( pFile->Attributes[ 0 ], 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

BYTE *T64FileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE Title[ 64 ];

	if ( pFile != nullptr )
	{
		rsprintf( Title, "T64:%s:%s.%s", pDir->Container, (BYTE *) pFile->Filename, (BYTE *) pFile->Extension );
	}
	else
	{
		rsprintf( Title, "T64:%s", pDir->Container );
	}

	return Title;
}

BYTE *T64FileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE Desc[ 128 ];

	NativeFile *pFile = &pDirectory->Files[ FileIndex ];

	const BYTE *ftypes[ 6 ] = { (BYTE *) "DELETED", (BYTE *) "SEQUENTIAL", (BYTE *) "PROGRAM", (BYTE *) "USER", (BYTE *) "RELATIVE", (BYTE *) "SNAPSHOT" };

	rsprintf( Desc, "%s FILE, %d BYTES, LOAD AT %04X", ftypes[ pFile->Attributes[ 3 ] ], (QWORD) pFile->Length, (QWORD) pFile->Attributes[ 2 ] );

	return Desc;
}

BYTE *T64FileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE Status[ 128 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( Status, "%d FILES", (int) pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( Status, "%d FILES SELECTED", SelectedItems );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[ FileIndex ];

		const BYTE *ftypes[ 6 ] = { (BYTE *) "DELETED", (BYTE *) "SEQUENTIAL", (BYTE *) "PROGRAM", (BYTE *) "USER", (BYTE *) "RELATIVE", (BYTE *) "SNAPSHOT" };

		rsprintf( Status, "%s.%s, %s FILE, %d BYTES, LOAD AT %04X", (BYTE *) pFile->Filename, (BYTE *) pFile->Extension, ftypes[ pFile->Attributes[ 3 ] ], (QWORD) pFile->Length, (QWORD) pFile->Attributes[ 2 ] );
	}

	return Status;
}

int T64FileSystem::MakeASCIIFilename( NativeFile *pFile )
{
	MakeASCII( pFile );

	return 0;
}

std::vector<AttrDesc> T64FileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Directory Offset. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Directory Offset";
	Attrs.push_back( Attr );

	/* Container Offset. Hex, visible, disabled */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Container Offset";
	Attrs.push_back( Attr );

	/* Load address. Hex, Visible */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load Address";
	Attrs.push_back( Attr );

	/* File type */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrDanger | AttrFile;
	Attr.Name  = L"File type";

	AttrOption opt;

	opt.Name            = L"Deleted (DEL)";
	opt.EquivalentValue = 0x00000000;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Sequential (SEQ)";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Program (PRG)";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"User (USR)";
	opt.EquivalentValue = 0x00000003;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Relative (REL)";
	opt.EquivalentValue = 0x00000004;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Snapshot (FRZ)";
	opt.EquivalentValue = 0x00000005;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );
		
	return Attrs;
}

std::vector<AttrDesc> T64FileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Directory Offset. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrEnabled | AttrString;
	Attr.Name  = L"Container Name";

	Attr.pStringVal = rstrndup( pDir->Container, 18 );

	Attrs.push_back( Attr );
		
	return Attrs;
}

int T64FileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	BYTE Entry[ 32 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pSource->ReadRaw( pFile->Attributes[ 0 ], 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}
	
	* (WORD *) &Entry[ 0x02 ] = (WORD) Changes->Attributes[ 2 ];
	
	if ( Changes->Attributes[ 3 ] == 0x05 )
	{
		Entry[ 0 ] = 3;
		Entry[ 1 ] = 0;
	}
	else
	{
		/* *sigh* */
		Entry[ 0 ] = 1;
		Entry[ 1 ] = (BYTE) Changes->Attributes[ 3 ] | 0x80;
	}

	if ( pSource->WriteRaw( pFile->Attributes[ 0 ], 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}
	
	return pDirectory->ReadDirectory();
}

int T64FileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	BYTE Entry[ 32 ];

	if ( pSource->ReadRaw( 0x20, 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}

	memset( &Entry[ 0x08 ], 0, 18 );

	rstrncpy( &Entry[ 0x08 ], pNewVal, 18 );

	TFromString( &Entry[ 0x08 ], 18 );

	if ( pSource->WriteRaw( 0x20, 32, Entry ) != DS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

int T64FileSystem::RegenerateSource( DWORD id1, DWORD id2, NativeFile *pIncoming, CTempFile *pStore, int Reason )
{
	CTempFile out;

	out.Seek( 0 );

	BYTE blk[ 32 ];

	/* Main header */
	ZeroMemory( blk, 32 );

	rsprintf( blk, "C64S tape image file" );

	out.Write( blk, 32 );

	/* Container header */
	ZeroMemory( blk, 32 );

	WORD NumFiles = pDirectory->Files.size();

	if ( Reason == T64_REGEN_DELETE ) { NumFiles--; }
	if ( Reason == T64_REGEN_WRITE  ) { NumFiles++; }

	* (WORD *) &blk[ 0x00 ] = pDir->TapeVersion;
	* (WORD *) &blk[ 0x02 ] = NumFiles;
	* (WORD *) &blk[ 0x04 ] = NumFiles;

	rstrncpy( &blk[ 0x08 ], pDir->Container, 18 );

	TFromString( &blk[ 0x08 ], 18 );

	out.Write( blk, 32 );

	DWORD DataOffset = 0x40 + ( NumFiles * 0x20 );
	WORD  DirOffset  = 0x40;
	DWORD FileID     = 0;

	NativeFileIterator iFile;

	NativeFile *pThisFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( ( Reason == T64_REGEN_DELETE ) && ( id1 == FileID ) )
		{
			FileID++;

			continue;
		}

		CTempFile oldFile;

		if ( Reason == T64_REGEN_SWAP )
		{
			if ( FileID == id1 ) 
			{
				ReadFile( id2, oldFile );

				pThisFile = &pDirectory->Files[ id2 ];
			}
			else if ( FileID == id2 )
			{
				ReadFile( id1, oldFile );

				pThisFile = &pDirectory->Files[ id1 ];
			}
			else
			{
				ReadFile( FileID, oldFile );

				pThisFile = &pDirectory->Files[ FileID ];
			}
		}
		else
		{
			ReadFile( FileID, oldFile );

			pThisFile = &pDirectory->Files[ FileID ];
		}

		DWORD RealLen   = oldFile.Ext() - 2;
		DWORD BytesToGo = RealLen;

		out.Seek( DataOffset );
		oldFile.Seek( 2 );

		while ( BytesToGo > 0 )
		{
			BYTE Buffer[ 0x4000 ];

			DWORD BytesWrite = min( 0x4000, BytesToGo );

			oldFile.Read( Buffer, BytesWrite );
			out.Write( Buffer, BytesWrite );

			BytesToGo -= BytesWrite;
		}

		out.Seek( DirOffset );

		/* Generate attribute block */
		ZeroMemory( blk, 32 );

		if ( iFile->Attributes[ 3 ] == 5 )
		{
			blk[ 0 ] = 3;
			blk[ 1 ] = 0;
		}
		else
		{
			blk[ 0 ] = 1;
			blk[ 1 ] = pThisFile->Attributes[ 3 ] | 0x80;
		}

		* (WORD *)  &blk[ 2 ] = pThisFile->Attributes[ 2 ];
		* (WORD *)  &blk[ 4 ] = ( pThisFile->Attributes[ 2 ] + RealLen ) - 1;
		* (DWORD *) &blk[ 8 ] = DataOffset;

		rstrncpy( &blk[ 0x10 ], pThisFile->Filename, 16 );

		TFromString( &blk[ 0x10 ], 16 );

		out.Write( blk, 32 );

		DirOffset  += 32;
		DataOffset += RealLen;

		FileID++;
	}

	/* Handle a file write (import) */
	if ( Reason == T64_REGEN_WRITE )
	{
		pStore->Seek( 0 );

		BYTE adx[ 2 ];

		pStore->Read( adx, 2 );

		DWORD BytesToGo = pStore->Ext();

		if ( pIncoming->FSFileType == FT_C64 )
		{
			/* Can't import FRZ, have to set it later */
			blk[ 0 ] = 1;
			blk[ 1 ] = pIncoming->Attributes[ 1 ] | 0x80;

			blk[ 2 ] = adx[ 0 ];
			blk[ 3 ] = adx[ 1 ];

			WORD StartAdx = * (WORD *) adx;

			* (WORD *) &blk[ 4 ] = ( StartAdx + BytesToGo ) - 1;

			BytesToGo -= 2;
		}
		else if ( pIncoming->FSFileType == FT_CBM_TAPE )
		{
			if ( pIncoming->Attributes[ 3 ] == 0x05 )
			{
				blk[ 0 ] = 3;
				blk[ 1 ] = 0;
			}
			else
			{
				blk[ 1 ] = pIncoming->Attributes[ 3 ];
				blk[ 0 ] = 1;
			}

			* (WORD *)  &blk[ 2 ] = (WORD) pIncoming->Attributes[ 2 ];
			* (WORD *)  &blk[ 4 ] = (WORD) ( pIncoming->Attributes[ 2 ] + BytesToGo ) - 1;

			BytesToGo -= 2;
		}
		else
		{
			blk[ 0 ] = 1;
			blk[ 1 ] = 0x82;

			/* Broad assumption here */
			if ( ( pIncoming->Flags & FF_Extension ) && ( rstrnicmp( pIncoming->Extension, (BYTE *) "PRG", 3 ) ) )
			{
				blk[ 2 ] = adx[ 0 ];
				blk[ 3 ] = adx[ 1 ];

				WORD StartAdx = * (WORD *) adx;

				* (WORD *) &blk[ 4 ] = ( StartAdx + BytesToGo ) - 1;
			}
			else
			{
				BytesToGo += 2;

				pStore->Seek( 0 );

				/* Fake this, let the user change the address later if needed */
				* (WORD *) &blk[ 2 ] = 0x0801;
				* (WORD *) &blk[ 4 ] = ( 0x0801 + BytesToGo ) - 1;
			}

		}

		* (DWORD *) &blk[ 8 ] = DataOffset;

		rstrncpy( &blk[ 0x10 ], pIncoming->Filename, 16 );

		TFromString( &blk[ 0x10 ], 16 );

		out.Seek( DirOffset );
		out.Write( blk, 32 );
		out.Seek( DataOffset );

		while ( BytesToGo > 0 )
		{
			BYTE Buffer[ 0x4000 ];

			DWORD BytesWrite = min( 0x4000, BytesToGo );

			pStore->Read( Buffer, BytesWrite );

			out.Write( Buffer, BytesWrite );

			BytesToGo -= BytesWrite;
		}
	}

	ReplaceSourceContent( pSource, out );

	return pDirectory->ReadDirectory();
}

int T64FileSystem::DeleteFile( DWORD FileID )
{
	return RegenerateSource( FileID, 0, nullptr, nullptr, T64_REGEN_DELETE );
}

int T64FileSystem::SwapFile( DWORD FileID1, DWORD FileID2 )
{
	return RegenerateSource( FileID1, FileID2, nullptr, nullptr, T64_REGEN_SWAP );
}

int T64FileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) L"Creating header..." );

	BYTE Header[ 0x40 ];

	ZeroMemory( Header, 0x40 );

	rsprintf( Header, "C64S tape image file" );
	
	Header[ 0x20 ] = 0x01;
	Header[ 0x21 ] = 0x01;
	
	rsprintf( &Header[ 0x28 ], "NUTS TAPE FILE" );

	pSource->WriteRaw( 0, 0x40, Header );

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 4, 1, 1, true ), (LPARAM) L"Completed" );

	return 0;
}

WCHAR *T64FileSystem::Identify( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	const WCHAR *ftypes[6] = { L"Deleted File Entry", L"Sequential Access File", L"Program", L"User File", L"Relative Access File", L"State Snapshot" };

	return (WCHAR *) ftypes[ pFile->Attributes[ 3 ] ];
}

int T64FileSystem::MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store )
{
	TapeLevel     = 0;
	TapePtr       = 0;
	TapeBufferPtr = 0;

	/* Set up the index sheet */
	indexes.Title     = L"T64 Image";
	indexes.Publisher = L"";

	NativeFileIterator iFile;

	store.Seek( 0 );

	BYTE Header[ 256 ];
	BYTE SyncBlock[ 9 ]  = { 0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81 };
	BYTE RSyncBlock[ 9 ] = { 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
	BYTE FileBuffer[ T64_TAPEAUDIO_BUFFER_SIZE ];

	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		if ( iFile->Attributes[ 2 ] != 0xFFFFFFFF )
		{
			TapeCue Cue;

			Cue.IndexPtr  = TapePtr;
			Cue.IndexName = std::wstring( UString( (char *) iFile->Filename ) );
			Cue.Flags      = 0;

			indexes.Cues.push_back( Cue );

			/* Pilot tone */
			for ( WORD pilot = 0; pilot<0x6A00; pilot++ )
			{
				WriteShortPulse( store );
			}

			WriteAudioBuffer( store, SyncBlock, 9 );

			memset( Header, 0, 192 );

			/* Because of course it is */
			if ( iFile->Attributes[ 3 ] == 0x01 )
			{
				Header[ 0 ] = 0x04;
			}
			else if ( iFile->Attributes[ 3 ] == 0x02 )
			{
				Header[ 0 ] = 0x01;
			}
			else
			{
				Header[ 0 ] = 0x03;
			}

			Header[ 0 ] = 0x01;

			* (WORD *) &Header[ 1 ] = (WORD) iFile->Attributes[ 2 ];
			* (WORD *) &Header[ 3 ] = (WORD) ( iFile->Attributes[ 2 ] + iFile->Length + 1 );
			
			memcpy( &Header[ 5 ], (BYTE *) iFile->Filename, 16 );

			TapeChecksum = 0;

			WriteAudioBuffer( store, Header, 192 );
			WriteAudioBuffer( store, &TapeChecksum, 1 );

			WriteAudioED( store );

			/* Repeat pilot */
			for ( WORD pilot = 0; pilot<0x4F; pilot++ )
			{
				WriteShortPulse( store );
			}

			WriteAudioBuffer( store, RSyncBlock, 9 );
			Header[ 0 ] = 0x01;

			TapeChecksum = 0;

			WriteAudioBuffer( store, Header, 192 );
			WriteAudioBuffer( store, &TapeChecksum, 1 );

			WriteAudioED( store );

			/* Header trailer pilot */
			for ( WORD pilot = 0; pilot<0x200; pilot++ )
			{
				WriteShortPulse( store );
			}

			/* Write a 0.4 second pause */
			WriteAudio( store, 127, 17640 );

			Cue.IndexPtr  = TapePtr;
			Cue.IndexName = L"Data Block";
			Cue.Extra     = L"Initial Data";
			Cue.Flags     = TF_ExtraValid | TF_Extra1;

			indexes.Cues.push_back( Cue );

			/* Data Pilot */
			for ( WORD pilot = 0; pilot<0x1A00; pilot++ )
			{
				WriteShortPulse( store );
			}

			WriteAudioBuffer( store, SyncBlock, 9 );

			CTempFile data;

			ReadFile( iFile->fileID, data );

			for ( int pass=0; pass<2; pass++ )
			{
				data.Seek( 0 );

				TapeChecksum = 0;

				DWORD DataToGo = data.Ext();

				while ( DataToGo > 0 )
				{
					DWORD DataBytes = min( DataToGo, T64_TAPEAUDIO_BUFFER_SIZE );

					data.Read( FileBuffer, DataBytes );

					WriteAudioBuffer( store, FileBuffer, DataBytes );

					DataToGo -= DataBytes;
				}

				if ( pass == 0 )
				{
					Cue.IndexPtr = TapePtr;
					Cue.Extra    = L"Repeated Data";
					Cue.Flags    = TF_ExtraValid | TF_Extra1;

					indexes.Cues.push_back( Cue );
				}

				WriteAudioBuffer( store, &TapeChecksum, 1 );

				WriteAudioED( store );

				/* Trailer pilot */
				for ( WORD pilot = 0; pilot<0x4F; pilot++ )
				{
					WriteShortPulse( store );
				}
			}

				/* Trailer pilot */
			for ( WORD pilot = 0; pilot<0x200; pilot++ )
			{
				WriteShortPulse( store );
			}

			/* Write a 4 second pause */
			WriteAudio( store, 127, 176400 );
		}
	}

	WriteAudio( store, 0, 0 );

	return 0;
}

void T64FileSystem::WriteShortPulse( CTempFile &output )
{
	WriteAudio( output, 127 + ( 127 * TapeLevel ), 9 );

	TapeLevel = 1 - TapeLevel;

	WriteAudio( output, 127 + ( 127 * TapeLevel ), 9 );

	TapeLevel = 1 - TapeLevel;
}

void T64FileSystem::WriteMediumPulse( CTempFile &output )
{
	WriteAudio( output, 127 + ( 127 * TapeLevel ), 12 );

	TapeLevel = 1 - TapeLevel;

	WriteAudio( output, 127 + ( 127 * TapeLevel ), 12 );

	TapeLevel = 1 - TapeLevel;
}

void T64FileSystem::WriteLongPulse( CTempFile &output )
{
	WriteAudio( output, 127 + ( 127 * TapeLevel ), 16 );

	TapeLevel = 1 - TapeLevel;

	WriteAudio( output, 127 + ( 127 * TapeLevel ), 16 );

	TapeLevel = 1 - TapeLevel;
}

void T64FileSystem::WriteAudio( CTempFile &output, BYTE sig, DWORD Length )
{
	for ( DWORD b=0; b<Length; b++ )
	{
		TapeBuffer[ TapeBufferPtr ] = sig;

		TapeBufferPtr++;
		TapePtr++;

		if ( TapeBufferPtr == T64_TAPEAUDIO_BUFFER_SIZE )
		{
			output.Write( TapeBuffer, TapeBufferPtr );

			TapeBufferPtr = 0;
		}
	}

	if ( ( Length == 0 ) && ( TapeBufferPtr > 0 ) )
	{
		output.Write( TapeBuffer, TapeBufferPtr );

		TapeBufferPtr = 0;
	}
}

void T64FileSystem::WriteAudioBuffer( CTempFile &output, BYTE *pBuffer, DWORD Length )
{
	for ( DWORD i=0; i<Length; i++ )
	{
		BYTE b = pBuffer[ i ];
		BYTE c = 1;

		TapeChecksum ^= b;

		WriteAudioDM( output );

		for ( BYTE bit=0; bit<8; bit++ )
		{
			if ( b & 1 )
			{
				WriteAudio1( output );

				c ^= 1;
			}
			else
			{
				WriteAudio0( output );

				c ^= 0;
			}

			b &=  0xFE;
			b >>= 1;
		}

		if( c == 1 )
		{
			WriteAudio1( output );
		}
		else
		{
			WriteAudio0( output );
		}
	}
}