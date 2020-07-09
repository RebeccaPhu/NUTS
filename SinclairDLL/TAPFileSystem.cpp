#include "StdAfx.h"
#include "SinclairDefs.h"
#include "TAPFileSystem.h"
#include "../NUTS/SourceFunctions.h"

TAPFileSystem::TAPFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) 
{
	pSource = pDataSource;

	FSID = FSID_SPECTRUM_TAP;

	Flags = FSF_Size | FSF_Supports_Spaces | FSF_Reorderable | FSF_Prohibit_Nesting;

	pDirectory = nullptr;
}


TAPFileSystem::~TAPFileSystem(void)
{
	if ( pDirectory != nullptr )
	{
		delete pDirectory;
	}
}

FSHint TAPFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension == nullptr )
	{
		return hint;
	}

	if ( rstrncmp( Extension, (BYTE *) "TAP", 3 ) )
	{
		hint.FSID       = FSID_SPECTRUM_TAP;
		hint.Confidence = 20;

		WORD FirstHeaderLen;

		pSource->ReadRaw( 0, 1, (BYTE *) &FirstHeaderLen );

		if ( FirstHeaderLen == 0x13 )
		{
			/* This indicates a spectrum header has been found - but this is not conclusive!
			
			   This could be a TAP file that uses custom loaders, and we're looking side B
			   which is data, and not intended to be loaded directly. */

			hint.Confidence = 30;
		}
	}

	return hint;
}

BYTE *TAPFileSystem::GetStatusString( int FileIndex, int SelectedItems )
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
	
	switch ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] )
	{
	case 0:
		{
			if ( pDirectory->Files[ FileIndex ].Attributes[ 3 ] < 32768 )
			{
				rsprintf(
					status, "Program: %s - %04X bytes LINE %d, vars: %04X",
					pDirectory->Files[ FileIndex ].Filename,
					(DWORD) pDirectory->Files[ FileIndex ].Length,
					pDirectory->Files[ FileIndex ].Attributes[ 3 ],
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
			else
			{
				rsprintf(
					status, "Program: %s - %04X bytes, vars: %04X",
					pDirectory->Files[ FileIndex ].Filename,
					(DWORD) pDirectory->Files[ FileIndex ].Length,
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
		}
		break;

	case 1:
		{
			rsprintf(
				status, "Number Array: %s - %04X bytes of %c$()",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 2:
		{
			rsprintf(
				status, "Character Array: %s - %04X bytes of %c$()",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 3:
		{
			rsprintf(
				status, "Bytes: %s - %04X bytes at %04X",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				pDirectory->Files[ FileIndex ].Attributes[ 1 ]
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

	return status;
}

BYTE *TAPFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[1024];

	switch ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] )
	{
	case 0:
		{
			if ( pDirectory->Files[ FileIndex ].Attributes[ 3 ] < 32768 )
			{
				rsprintf(
					status, "Program: LINE %d, vars at offset %04X",
					pDirectory->Files[ FileIndex ].Attributes[ 3 ],
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
			else
			{
				rsprintf(
					status, "Program: Vars at offset %04X",
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
		}
		break;

	case 1:
		{
			rsprintf(
				status, "Number Array: %04X bytes of %c$()",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 2:
		{
			rsprintf(
				status, "Character Array: %04X bytes of %c$()",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 3:
		{
			rsprintf(
				status, "Bytes: %04X bytes at %04X",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				pDirectory->Files[ FileIndex ].Attributes[ 1 ]
			);
		}
		break;

	default:
		{
			rsprintf(
				status, "Orphaned block (No header) - %04X bytes",
				UString( (char *) pDirectory->Files[ FileIndex ].Filename ),
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;
	}

	return status;
}


BYTE *TAPFileSystem::GetTitleString( NativeFile *pFile = nullptr )
{
	static BYTE title[ 1024 ];
	
	if ( pFile != nullptr )
	{
		rsprintf( title, "%s", pFile->Filename );
	}
	else
	{
		rsprintf( title, (BYTE *) "TAP Image" );
	}

	return title;
}


int TAPFileSystem::ReadFile(DWORD FileID, CTempFile &store) {
	BYTE *pBuffer = (BYTE *) malloc( (size_t) pDirectory->Files[ FileID ].Length );

	pSource->ReadRaw(
		pDirectory->Files[ FileID ].Attributes[ 0 ] + 3,
		(DWORD) pDirectory->Files[ FileID ].Length,
		pBuffer
	);

	store.Seek( 0 );
	store.Write( pBuffer, (DWORD) pDirectory->Files[ FileID ].Length );

	free( pBuffer );

	return 0;
}

inline BYTE TAPFileSystem::TAPSum( BYTE *Blk, DWORD Bytes )
{
	BYTE x = 0;

	for ( DWORD i=0; i<Bytes; i++) 
	{
		x ^= Blk[ i ];
	}

	return x;
}

int	TAPFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_SINCLAIR ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	DWORD FileEnd = 0;

	if ( pDirectory->Files.size() > 0 )
	{
		FileEnd = pDirectory->Files.back().Attributes[ 0 ] + 3 + pDirectory->Files.back().Length + 1;
	}

	return WriteAtStore( pFile, store, nullptr, FileEnd );
}

int	TAPFileSystem::WriteAtStore(NativeFile *pFile, CTempFile &store, CTempFile *output, DWORD Offset)
{
	DWORD FilePos = Offset;

	BYTE Header[ 21 ] = { 0x13, 0x00, 0x00 };

	ZeroMemory( &Header[ 0x03 ], 18 );

	memset( &Header[ 0x04 ], 0x20, 10 );
	rstrncpy( &Header[ 0x04 ], pFile->Filename, 10 );

	for ( BYTE i=0x04; i<0x0E; i++ )
	{
		if ( Header[ i ] == 0 ) { Header[ i ] = 0x20; }
	}

	* (WORD *) &Header[ 14 ] = pFile->Length;

	if ( pFile->FSFileType == FT_SINCLAIR )
	{
		Header[ 0x03 ] = (BYTE) pFile->Attributes[ 2 ];

		if ( pFile->Attributes[ 2 ] != 0xFFFFFFFF )
		{
			switch ( pFile->Attributes[ 2 ] )
			{
				case 0:
					* (WORD *) &Header[ 16 ] = pFile->Attributes[ 3 ];
					* (WORD *) &Header[ 18 ] = pFile->Attributes[ 4 ];

					break;

				case 1:
				case 2:
					Header[17] = pFile->Attributes[ 5 ];

					break;

				case 3:
					* (WORD *) &Header[ 16 ] = pFile->Attributes[ 1 ];
					* (WORD *) &Header[ 18 ] = 32768;

					break;
			}

			Header[ 20 ] = TAPSum( &Header[ 2 ], 18 );

			if ( output != nullptr )
			{
				output->Seek( FilePos );
				output->Write( Header, 21 );
			}
			else
			{
				pSource->WriteRaw( FilePos, 21, Header );
			}

			FilePos += 21;
		}
	}
	else if ( pFile->FSFileType == FT_SINCLAIR_DOS )
	{
		Header[ 0x03 ] = (BYTE) pFile->Attributes[ 6 ];

		if ( pFile->Attributes[ 6 ] != 0xFFFFFFFF )
		{
			switch ( pFile->Attributes[ 6 ] )
			{
				case 0:
					* (WORD *) &Header[ 16 ] = pFile->Attributes[ 7 ];
					* (WORD *) &Header[ 18 ] = pFile->Attributes[ 8 ];

					break;

				case 1:
				case 2:
					Header[17] = pFile->Attributes[ 5 ];

					break;

				case 3:
					* (WORD *) &Header[ 16 ] = pFile->Attributes[ 5 ];
					* (WORD *) &Header[ 18 ] = 32768;

					break;
			}

			Header[ 20 ] = TAPSum( &Header[ 2 ], 18 );

			if ( output != nullptr )
			{
				output->Seek( FilePos );
				output->Write( Header, 21 );
			}
			else
			{
				pSource->WriteRaw( FilePos, 21, Header );
			}

			FilePos += 21;
		}
	}
	else
	{
		/* Pretend it's bytes at 0x4000 */
		Header[ 0x03 ] = 3;

		* (WORD *) &Header[ 16 ] = 0x4000;
		* (WORD *) &Header[ 18 ] = 32768;

		Header[ 20 ] = TAPSum( &Header[ 2 ], 18 );

		if ( output != nullptr )
		{
			output->Seek( FilePos );
			output->Write( Header, 21 );
		}
		else
		{
			pSource->WriteRaw( FilePos, 21, Header );
		}

		FilePos += 21;
	}

	/* Now the file content itself */
	DWORD Sz = store.Ext();

	BYTE *pData = (BYTE *) malloc( Sz + 3);

	* (WORD *) &pData[ 0x00 ] = Sz + 2;

	pData[ 0x02 ] = 0xFF;

	store.Seek( 0 );
	store.Read( &pData[ 0x03 ], Sz );

	BYTE x = TAPSum( &pData[ 2 ], Sz + 1 );

	if ( output != nullptr )
	{
		output->Seek( FilePos );
		output->Write( pData, Sz + 3 );
	}
	else
	{
		pSource->WriteRaw( FilePos, Sz + 3, pData );
	}

	FilePos += Sz + 3;

	if ( output != nullptr )
	{
		output->Seek( FilePos );
		output->Write( &x, 1 );
	}
	else
	{
		pSource->WriteRaw( FilePos, 1, &x );
	}

	FilePos++;

	free( pData );

	/* No need to add to the directory, just re-read it */

	if ( output != nullptr )
	{
		return FilePos - Offset;
	}

	return pDirectory->ReadDirectory();
}

int TAPFileSystem::RewriteTAPFile( DWORD SpecialID, DWORD SwapID, BYTE *pName, int Reason )
{
	CTempFile Rewritten;
	CTempFile OldFile;

	NativeFileIterator iFile;

	DWORD Pos = 0;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( ( Reason == REASON_DELETE ) && ( iFile->fileID == SpecialID ) )
		{
			continue;
		}

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
			rstrncpy( file.Filename, pName, 10 );
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

AttrDescriptors TAPFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Offset in TAP file. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex;
	Attr.Name  = L"TAP Offset";
	Attrs.push_back( Attr );

	/* File Type */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrWarning | AttrFile;
	Attr.Name  = L"File Type";
	
	AttrOption opt;

	opt.Dangerous       = false;
	opt.EquivalentValue = 0;
	opt.Name            = L"Program";
	
	Attr.Options.push_back( opt );

	opt.EquivalentValue = 1;
	opt.Name            = L"Number Array";

	Attr.Options.push_back( opt );

	opt.EquivalentValue = 2;
	opt.Name            = L"Character Array";

	Attr.Options.push_back( opt );

	opt.EquivalentValue = 3;
	opt.Name            = L"Bytes";

	Attr.Options.push_back( opt );

	opt.Dangerous       = true;
	opt.EquivalentValue = 0xFFFFFFFF;
	opt.Name            = L"Orphaned Block";

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );

	/* Load addr for bytes */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load Address";
	Attrs.push_back( Attr );

	/* Auto start line */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrDec | AttrWarning | AttrFile;
	Attr.Name  = L"Auto start line";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Vars Offset";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrDanger | AttrFile;;
	Attr.Name  = L"Variable ID";
	Attrs.push_back( Attr );

	return Attrs;
}

int TAPFileSystem::DeleteFile( NativeFile *pFile, int FileOp )
{
	/* Note: FileOp is only ever FILEOP_DELETE because we don't complain if the filename already
	   exists. */

	return RewriteTAPFile( pFile->fileID, 0, nullptr, REASON_DELETE );
}

int TAPFileSystem::Rename( DWORD FileID, BYTE *NewName )
{
	if ( pDirectory->Files[ FileID ].Attributes[ 2 ] == 0xFFFFFFFF )
	{
		return NUTSError( 0x45, L"The file selected is an Orphaned Block, and does not have a filename, thus it cannot be renamed. To assign a name to this block, use the properties page to change the block to a full file, then try again." );
	}

	return RewriteTAPFile( FileID, 0, NewName, REASON_RENAME );
}

int TAPFileSystem::SwapFile( DWORD FileID1, DWORD FileID2 )
{
	return RewriteTAPFile( FileID1, FileID2, nullptr, REASON_SWAP );
}

int TAPFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	if ( pDirectory->Files.size() > FileID )
	{
		pDirectory->Files[ FileID ] = *Changes;
	}

	return RewriteTAPFile( FileID, 0, nullptr, REASON_PROPS );
}