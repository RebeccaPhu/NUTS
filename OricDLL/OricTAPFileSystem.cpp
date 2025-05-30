#include "StdAfx.h"
#include "OricTAPFileSystem.h"

#include "OricDefs.h"
#include "..\NUTS\NativeFile.h"
#include "..\NUTS\SourceFunctions.h"

OricTAPFileSystem::OricTAPFileSystem( DataSource *pSrc ) : FileSystem( pSrc )
{
	pOricDir = new OricTAPDirectory( pSrc );

	pDirectory = (Directory *) pOricDir;

	TopicIcon = FT_TapeImage;

	Flags = FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_DynamicSize | FSF_Reorderable | FSF_Prohibit_Nesting;
}


OricTAPFileSystem::~OricTAPFileSystem(void)
{
	if ( pOricDir != nullptr )
	{
		delete pOricDir;
	}

	pOricDir = nullptr;
}

FSHint OricTAPFileSystem::Offer( BYTE *Extension )
{
	BYTE Header[ 128 ];

	ZeroMemory( Header, 128 );

	pSource->ReadRaw( 0, 128, Header );

	BYTE p=0;

	FSHint hint;

	hint.FSID       = ORIC_TAP;
	hint.Confidence = 0;

	// TAP Files have no definitively identifying marks, but there is a
	// sequence of 0x16 bytes (typically 3, 8 or 16) followed by the sync
	// byte of 0x24. We'll key on this.

	while ( ( Header[ p ] == 0x16 ) && ( p < 126 ) )
	{
		p++;
	}

	if ( Header[ p ] == 0x24 )
	{
		hint.Confidence += 15;
	}

	return hint;
}

std::vector<AttrDesc> OricTAPFileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* File type, BASIC or machine code */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrFile | AttrWarning;
	Attr.Name  = L"File Type";

	static DWORD TypeValues[ ] = {
		ORIC_TAP_FILE_BASIC, ORIC_TAP_FILE_MCODE, ORIC_TAP_FILE_AR_IN, ORIC_TAP_FILE_AR_RL, ORIC_TAP_FILE_AR_ST
	};

	static std::wstring TypeNames[ ] = {
		L"BASIC", L"MachineCode", L"Integer Array", L"Real Array", L"String Array"
	};

	Attr.Options.clear();

	for ( BYTE i=0; i<5; i++ )
	{
		AttrOption opt;

		opt.Name            = TypeNames[ i ];
		opt.EquivalentValue = TypeValues[ i ];
		opt.Dangerous       = true;

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	/* Write */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Autorun";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Raw Offset Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Raw Offset";
	Attrs.push_back( Attr );

	return Attrs;
}

int OricTAPFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = ( NativeFile *) &pDirectory->Files[ FileID ];

	store.Seek( 0 );

	DWORD BytesToGo = (DWORD) pFile->Length;

	QWORD Offset = pFile->Attributes[ 4 ];

	AutoBuffer buf( 32768 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 32768, (DWORD) BytesToGo );

		if ( pSource->ReadRaw( Offset, BytesRead, (BYTE *) buf ) != DS_SUCCESS )
		{
			return -1;
		}

		store.Write( (BYTE *) buf, BytesRead );

		BytesToGo -= BytesRead;
		Offset    += BytesRead;
	}

	return NUTS_SUCCESS;
}

#define REASON_DELETE 0
#define REASON_RENAME 1
#define REASON_SWAP   2
#define REASON_PROPS  3

int OricTAPFileSystem::RewriteTAPFile( DWORD SpecialID, DWORD SwapID, BYTE *pName, int Reason )
{
	CTempFile Rewritten;

	NativeFileIterator iFile;

	DWORD Pos = 0;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( ( Reason == REASON_DELETE ) && ( iFile->fileID == SpecialID ) )
		{
			continue;
		}

		CTempFile OldFile;

		NativeFile file = *iFile;

		if ( Reason == REASON_SWAP )
		{
			if ( iFile->fileID == SpecialID )
			{
				ReadFile( SwapID, OldFile );

				file = pDirectory->Files[ SwapID ];
			}
			else if ( iFile->fileID == SwapID )
			{
				ReadFile( SpecialID, OldFile );

				file = pDirectory->Files[ SpecialID ];
			}
			else
			{
				ReadFile( iFile->fileID, OldFile );

				file = pDirectory->Files[ iFile->fileID ];
			}
		}
		else
		{
			ReadFile( iFile->fileID, OldFile );

			file = pDirectory->Files[ iFile->fileID ];
		}

		if ( ( Reason == REASON_RENAME ) && ( iFile->fileID == SpecialID ) )
		{
			file.Filename = BYTEString( pName, 10 );
		}

		int Copied = WriteAtStore( &file, OldFile, &Rewritten, Pos );

		if ( Copied < 0 )
		{
			return -1;
		}

		Pos += Copied;
	}

	ReplaceSourceContent( pSource, Rewritten );

	return pDirectory->ReadDirectory();
}

int	OricTAPFileSystem::WriteAtStore(NativeFile *pFile, CTempFile &store, CTempFile *output, DWORD Offset)
{
	DWORD FilePos = Offset;

	BYTE Header[ 64 ];

	ZeroMemory( Header, 64 );

	if ( pFile->FSFileType == FT_ORIC )
	{
		BYTE Syncs = 4;
		DWORD hp   = 0;

		memset( Header, 0x16, Syncs );
		
		hp += Syncs;

		Header[ hp++ ] = 0x24;
		
		WBETWORD( &Header[ hp ], pFile->Attributes[ 1 ] ); hp += 3;

		Header[ hp++ ] = pFile->Attributes[ 2 ] & 0xFF;

		WORD EndAddr = (WORD) ( pFile->Attributes[ 3 ] + (DWORD) pFile->Length ); EndAddr--;

		WBEWORD( &Header[ hp ], EndAddr ); hp += 2;
		WBEWORD( &Header[ hp ], pFile->Attributes[ 3 ] ); hp += 2;

		Header[ hp++ ] = 0;

		rstrncpy( &Header[ hp ], (BYTE *) pFile->Filename, 15 );

		hp += rstrnlen( (BYTE *) pFile->Filename, 15 );

		Header[ hp++ ] = 0;

		if ( output != nullptr )
		{
			output->Seek( FilePos );
			output->Write( Header, hp );
		}
		else
		{
			pSource->WriteRaw( FilePos, hp, Header );
		}

		FilePos += hp;
	}
	else
	{
		// Fake a file loading at 0x0500 ?
		DWORD hp = 4;

		memset( Header, 0x16, 4 );
		
		Header[ hp++ ] = 0x24;
		
		WBETWORD( &Header[ hp ], 0x000080 ); hp += 3;

		Header[ hp++ ] = 0x00; // No autorun (probably best...)

		WORD EndAddr = 0x500 + (WORD) pFile->Length; EndAddr--;

		WBEWORD( &Header[ hp ], EndAddr ); hp += 2;
		WBEWORD( &Header[ hp ], 0x0500 ); hp += 2;

		Header[ hp++ ] = 0;

		rstrncpy( &Header[ hp ], (BYTE *) pFile->Filename, 15 );

		hp += rstrnlen( (BYTE *) pFile->Filename, 15 );

		Header[ hp++ ] = 0;

		if ( output != nullptr )
		{
			output->Seek( FilePos );
			output->Write( Header, hp );
		}
		else
		{
			pSource->WriteRaw( FilePos, hp, Header );
		}

		FilePos += hp;
	}

	/* Now the file content itself */
	DWORD Sz = (DWORD) store.Ext();

	AutoBuffer Content( Sz );

	store.Seek( 0 );
	store.Read( (BYTE *) Content, Sz );

	if ( output != nullptr )
	{
		output->Seek( FilePos );
		output->Write( (BYTE *) Content, Sz );
	}
	else
	{
		pSource->WriteRaw( FilePos, Sz, (BYTE *) Content );
	}

	FilePos += Sz;

	/* No need to add to the directory, just re-read it */

	if ( output != nullptr )
	{
		return FilePos - Offset;
	}

	return pDirectory->ReadDirectory();
}

int	OricTAPFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ORIC ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( pFile->Length > 0xFFFF )
	{
		return NUTSError( 0xE09, L"This file is too large to represent in a standard Oric TAP header" );
	}

	DWORD FileEnd = 0;

	if ( pDirectory->Files.size() > 0 )
	{
		FileEnd = pDirectory->Files.back().Attributes[ 4 ] + (DWORD) pDirectory->Files.back().Length;
	}

	return WriteAtStore( pFile, store, nullptr, FileEnd );
}

int OricTAPFileSystem::SwapFile( DWORD FileID1, DWORD FileID2 )
{
	return RewriteTAPFile( FileID1, FileID2, nullptr, REASON_SWAP );
}

int OricTAPFileSystem::DeleteFile( DWORD FileID )
{
	/* Note: FileOp is only ever FILEOP_DELETE because we don't complain if the filename already
	   exists. */

	return RewriteTAPFile( FileID, 0, nullptr, REASON_DELETE );
}

int OricTAPFileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
{
	return RewriteTAPFile( FileID, 0, NewName, REASON_RENAME );
}

int OricTAPFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	if ( pDirectory->Files.size() > FileID )
	{
		pDirectory->Files[ FileID ] = *Changes;
	}

	return RewriteTAPFile( FileID, 0, nullptr, REASON_PROPS );
}


BYTE *OricTAPFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE status[ 128 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );

		return status;
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );

		return status;
	}
	
	switch ( pDirectory->Files[ FileIndex ].Attributes[ 1 ] )
	{
	case ORIC_TAP_FILE_BASIC:
		{
			rsprintf(
				status, "BASIC: %s - %02X b @ %02X%s",
				(BYTE *) pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				(WORD) pDirectory->Files[ FileIndex ].Attributes[ 3 ],
				( (pDirectory->Files[ FileIndex ].Attributes[ 2 ] & 0xC7)?(BYTE*)", Autorun":(BYTE*)"")
			);
		}
		break;

	case ORIC_TAP_FILE_MCODE:
		{
			rsprintf(
				status, "M/Code: %s - %02X b @ %02X%s",
				(BYTE *) pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				(WORD) pDirectory->Files[ FileIndex ].Attributes[ 3 ],
				( (pDirectory->Files[ FileIndex ].Attributes[ 2 ] & 0xC7)?(BYTE*)", Autorun":(BYTE*)"")
			);
		}
		break;

	case ORIC_TAP_FILE_AR_IN:
		{
			rsprintf(
				status, "Integer Array: %s - %d integers",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) ( pDirectory->Files[ FileIndex ].Length / 2)
			);
		}
		break;

	case ORIC_TAP_FILE_AR_RL:
		{
			rsprintf(
				status, "Real Array: %s - %d reals",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;

	case ORIC_TAP_FILE_AR_ST:
		{
			rsprintf(
				status, "String Array: %s",
				pDirectory->Files[ FileIndex ].Filename
			);
		}
		break;

	default:
		{
			rsprintf(
				status, "%s - %04X bytes",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;
	}

	if ( pDirectory->Files[ FileIndex ].Attributes[ 0 ] > 0 )
	{
		rstrncat( status, (BYTE*) " (FAST)", 128 );
	}
	else
	{
		rstrncat( status, (BYTE*) " (SLOW)", 128 );
	}

	return status;
}

BYTE *OricTAPFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[1024];

	switch ( pDirectory->Files[ FileIndex ].Attributes[ 1 ] )
	{
	case ORIC_TAP_FILE_BASIC:
		{
			rsprintf(
				status, "BASIC: %02X bytes @ %02X, Autorun: %s",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				(WORD) pDirectory->Files[ FileIndex ].Attributes[ 3 ],
				( (pDirectory->Files[ FileIndex ].Attributes[ 2 ] & 0xC7)?(BYTE*)"Yes":(BYTE*)"No")
			);
		}
		break;

	case ORIC_TAP_FILE_MCODE:
		{
			rsprintf(
				status, "Machine Code: %02X bytes @ %02X, Autorun: %s",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				(WORD) pDirectory->Files[ FileIndex ].Attributes[ 3 ],
				( (pDirectory->Files[ FileIndex ].Attributes[ 2 ] & 0xC7)?(BYTE*)"Yes":(BYTE*)"No")
			);
		}
		break;

	case ORIC_TAP_FILE_AR_IN:
		{
			rsprintf(
				status, "Integer Array: %d integers",
				(DWORD) ( pDirectory->Files[ FileIndex ].Length / 2)
			);
		}
		break;

	case ORIC_TAP_FILE_AR_RL:
		{
			rsprintf(
				status, "Real Array: %d reals",
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;

	case ORIC_TAP_FILE_AR_ST:
		{
			rsprintf(
				status, "String Array"
			);
		}
		break;

	default:
		{
			rsprintf(
				status, "%04X bytes",
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;
	}


	if ( pDirectory->Files[ FileIndex ].Attributes[ 0 ] > 0 )
	{
		rstrncat( status, (BYTE*) " (FAST)", 128 );
	}
	else
	{
		rstrncat( status, (BYTE*) " (SLOW)", 128 );
	}

	return status;
}


BYTE *OricTAPFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE title[ 1024 ];
	
	if ( pFile != nullptr )
	{
		rsprintf( title, "%s", (BYTE *) pFile->Filename );
	}
	else
	{
		rsprintf( title, (BYTE *) "Oric TAPE Image" );
	}

	return title;
}

int OricTAPFileSystem::MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store )
{
	store.Seek( 0 );

	TapePartial = 0.0;
	TapePtr     = 0;
	TapeFilePtr = 0;

	indexes.Title     = L"Oric Cassette";
	indexes.Publisher = L"";

	for ( NativeFileIterator iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		// First generate a header
		BYTE Header[ 512 ];

		ZeroMemory( Header, 512 );

		WORD  Syncs = 259;
		DWORD hp    = 0;
		bool  FE    = false;

		TapeCue Cue;

		Cue.Flags     = 0;
		Cue.IndexPtr  = TapeFilePtr;
		Cue.Flags     = TF_ExtraValid | TF_Extra1;
		Cue.IndexName = std::wstring( UString( (char *) iFile->Filename ) );

		if ( FE )
		{
			Cue.Extra = L"Oric File, Fast Encoding";
		}
		else
		{
			Cue.Extra = L"Oric File, Slow Encoding";
		}

		indexes.Cues.push_back( Cue );

		memset( Header, 0x16, Syncs );
		
		hp += Syncs;

		Header[ hp++ ] = 0x24;
		
		WBETWORD( &Header[ hp ], iFile->Attributes[ 1 ] ); hp += 3;

		Header[ hp++ ] = iFile->Attributes[ 2 ] & 0xFF;

		WORD EndAddr = (WORD) ( iFile->Attributes[ 3 ] + (DWORD) iFile->Length ); EndAddr--;

		WBEWORD( &Header[ hp ], EndAddr ); hp += 2;
		WBEWORD( &Header[ hp ], iFile->Attributes[ 3 ] ); hp += 2;

		Header[ hp++ ] = 0;

		rstrncpy( &Header[ hp ], (BYTE *) iFile->Filename, 15 );

		hp += rstrnlen( (BYTE *) iFile->Filename, 15 );

		Header[ hp++ ] = 0;

		WriteAudioBytes( Header, hp, store, FE );

		AutoBuffer buf( 4096 );

		DWORD BytesToGo = (DWORD) iFile->Length;
		DWORD Offset    = iFile->Attributes[ 4 ];

		while ( BytesToGo > 0 )
		{
			DWORD ReadBytes = min( BytesToGo, 4096 );

			if ( pSource->ReadRaw( Offset, ReadBytes, (BYTE *) buf ) != DS_SUCCESS )
			{
				return -1;
			}

			WriteAudioBytes( (BYTE *) buf, ReadBytes, store, FE );

			BytesToGo -= ReadBytes;
			Offset    += ReadBytes;
		}

		memset( (BYTE *) buf, 127, 4096 );

		for ( int i = 0; i<40; i++ )
		{
			WriteAudioBuffer( store, (BYTE *) buf, 4096 );
		}
	}

	if ( TapePtr > 0 )
	{
		store.Write( TapeBuffer, TapePtr );
	}

	return 0;
}

void OricTAPFileSystem::WriteAudioBytes( BYTE *buf, DWORD Sz, CTempFile &store, bool FE )
{
	for ( DWORD b=0; b<Sz; b++ )
	{
		  int i;
		  int parity = 1;

		  BYTE v = buf[ b ];

		  WriteAudioBit( store, 0, FE );

		  for ( i = 0; i < 8; i++, v >>= 1 )
		  {
			parity += v & 1;

			WriteAudioBit( store, v & 1, FE );
		  }

		  WriteAudioBit( store, parity & 1, FE );

		  WriteAudioBit( store, 1, FE );
		  WriteAudioBit( store, 1, FE );
		  WriteAudioBit( store, 1, FE );
	}
}

void OricTAPFileSystem::WriteAudioBit( CTempFile &store, BYTE bit, bool FE )
{
	if ( FE )
	{
		// Fast encoding
		if ( bit == 1 )
		{
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
		}
		else
		{
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.41 );
		}
	}
	else
	{
		// Slow encoding
		if ( bit == 0 )
		{
			WriteAudioLevel( store, 1, 0.41 );
			WriteAudioLevel( store, 0, 0.41 );
			WriteAudioLevel( store, 1, 0.41 );
			WriteAudioLevel( store, 0, 0.41 );
			WriteAudioLevel( store, 1, 0.41 );
			WriteAudioLevel( store, 0, 0.41 );
			WriteAudioLevel( store, 1, 0.41 );
			WriteAudioLevel( store, 0, 0.41 );
		}
		else
		{
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
			WriteAudioLevel( store, 1, 0.205 );
			WriteAudioLevel( store, 0, 0.205 );
		}
	}
}

void OricTAPFileSystem::WriteAudioLevel( CTempFile &store, BYTE level, double Len )
{
	// Len is time in ms. When converted to samples at 44.1kHz, this may result in partials.
	double samples = ( 44100.0 / 1000.0 ) * Len;

	// samples is now the number of (floating point) samples required to represent the level
	while ( TapePartial > 1.0 )
	{
		// Correct for previous partials
		samples += 1.0;
		
		TapePartial -= 1.0;
	}

	TapePartial += samples - floor( samples );

	int iSamples = (int) floor( samples );

	static BYTE buf[ 32 ];

	for (int i=0; i<iSamples; i++)
	{
		if ( level )
		{
			buf[ i ] = 0xFF;
		}
		else
		{
			buf[ i ] = 0x7F;
		}
	}

	WriteAudioBuffer( store, buf, iSamples );
}

void OricTAPFileSystem::WriteAudioBuffer( CTempFile &store, BYTE *b, DWORD Sz )
{
	if ( ( TapePtr + Sz ) > ORIC_TAPE_BUFFER_SIZE )
	{
		DWORD cut = ORIC_TAPE_BUFFER_SIZE - TapePtr;

		memcpy( &TapeBuffer[ TapePtr ], b, cut );

		store.Write( TapeBuffer, ORIC_TAPE_BUFFER_SIZE );

		memcpy( TapeBuffer, &b[ cut ], Sz - cut );

		TapePtr = Sz - cut;
	}
	else
	{
		memcpy( &TapeBuffer[ TapePtr ], b, Sz );

		TapePtr += Sz;
	}

	TapeFilePtr += Sz;
}