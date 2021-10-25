#include "StdAfx.h"
#include "TZXFileSystem.h"

#include "../NUTS/Preference.h"
#include "../NUTS/SourceFunctions.h"
#include "../TZXConfig/TZXHardware.h"

#define round(a) ( (a-floor(a)>=0.5)?(floor(a)+1):(floor(a)) )

FSHint TZXFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = tzxpb.FSID;

	TopicIcon = FT_TapeImage;

	if ( Extension != nullptr )
	{
		if ( rstrnicmp( Extension, tzxpb.Extension, 3 ) )
		{
			hint.Confidence += 10;
		}
	}

	BYTE Header[ 0x0A ];

	pSource->ReadRaw( 0, 0x0A, Header );

	if ( rstrnicmp( Header, (BYTE *) "ZXTape!\x1A", 8 ) )
	{
		hint.Confidence += 20;
	}

	return hint;
}

int TZXFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	BYTE Buffer[ 0x4000 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true ) )
	{
		if ( (DWORD) Preference( tzxpb.RegistryPrefix + L"_ExportTextOption", (DWORD) 0 ) == (DWORD) 1 )
		{
			if ( pFile->Type == FT_Text )
			{
				return ReadTextBlock( pFile, store );
			}
		}
	}

	DWORD BytesToGo = pFile->Length;

	QWORD Offset = pFile->Attributes[ 15 ];

	if ( Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true ) )
	{
		if ( (DWORD) Preference( tzxpb.RegistryPrefix + L"_ExportDataOption", (DWORD) 0 ) == (DWORD) 1 )
		{
			BYTE BlockID = pFile->Attributes[ 14 ];

			if ( BlockID == 0x10 )
			{
				Offset    += 0x05;
				BytesToGo -= 0x05;
			}

			if ( BlockID == 0x11 )
			{
				Offset    += 0x13;
				BytesToGo -= 0x13;
			}

			if ( BlockID == 0x14 )
			{
				Offset    += 0x0B;
				BytesToGo -= 0x0B;
			}
		}
	}

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 0x4000, BytesToGo );

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

int TZXFileSystem::ReadBlock(DWORD FileID, CTempFile &store)
{
	BYTE Buffer[ 0x4000 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	DWORD BytesToGo = pDir->BlockLengths[ FileID ];

	QWORD Offset = pFile->Attributes[ 15 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 0x4000, BytesToGo );

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

LocalCommands TZXFileSystem::GetLocalCommands( void )
{
	LocalCommand c1 = { LC_Always,      L"Create new block" }; 
	LocalCommand c2 = { LC_ApplyOne,    L"Edit block" };
	LocalCommand s  = { LC_IsSeparator, L"" };
	LocalCommand c3 = { LC_OneOrMore,   L"Speedlock Bit Fix" };
	LocalCommand c4 = { LC_ApplyOne,    L"Convert to standard block" };
	LocalCommand c5 = { LC_ApplyOne,    L"Convert to turbo block" };
	LocalCommand c6 = { LC_ApplyOne,    L"Convert to pure data block" };

	LocalCommand c7 = { LC_Always,      L"Import/Export Options" };
	LocalCommand c8 = { LC_Always,      L"Resolve files" };

	LocalCommands cmds;

	cmds.HasCommandSet = true;
	cmds.Root          = tzxpb.LocalMenu;

	if ( Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true ) )
	{
		c8.Flags |= LC_IsChecked;
	}

	cmds.CommandSet.push_back( c1 );
	cmds.CommandSet.push_back( c2 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c3 );
	cmds.CommandSet.push_back( c4 );
	cmds.CommandSet.push_back( c5 );
	cmds.CommandSet.push_back( c6 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c7 );
	cmds.CommandSet.push_back( c8 );

	return cmds;
}

int TZXFileSystem::ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
{
	switch ( CmdIndex )
	{
	case 0:
		{
			BYTE BlockID = TZXSelectNewBlock( hParentWnd );

			bool Resolve = Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true );

			if ( Resolve )
			{
				if ( ( BlockID == 0x23 ) || ( BlockID == 0x26 ) || ( BlockID == 0x28 ) )
				{
					MessageBox( hParentWnd,
						L"The block cannot be edited correctly while resolved files is enabled, as the block editor is "
						L"unable to determine the correct block offsets. To edit this block, disable Resolve Files first.",
						L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

					return 0;
				}
			}

			if ( BlockID == 0xFF )
			{
				return 0;
			}

			if ( BlockID == 0x5A )
			{
				if ( MessageBox( hParentWnd,
					L"The glue block is a fudge in order to join TZX files together, and should not normally be used (though it is harmless to do so)\r\n"
					L"Are you sure you want to create a glue block?",
					L"NUTS TZX Configuration", MB_ICONQUESTION | MB_YESNO ) == IDNO )
				{
					return 0;
				}
			}

			if ( ( BlockID == 0x15 ) || ( BlockID == 0x18 ) || ( BlockID == 0x19 ) ) 
			{
				MessageBox( hParentWnd,
					L"The selected block is a complex recording block whose editing is beyond the scope of this application.",
					L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

				return 0;
			}

			CTempFile store;

			store.Write( &BlockID, 1 );

			bool NoConfig = false;

			if ( ( BlockID == 0x22 ) || ( BlockID == 0x25 ) || ( BlockID == 0x27 ) || ( BlockID == 0x2A ) || ( BlockID == 0x5A ) )
			{
				NoConfig = true;
			}

			if ( !NoConfig )
			{
				int r = TZXConfigureBlock( hParentWnd, 0, BlockID, &store, true, true, pDirectory->Files );

				if ( r < 0 )
				{
					return 0;
				}
			}

			int p = TZXSelectBlock( hParentWnd, pDirectory->Files );

			if ( p < 0 )
			{
				return 0;
			}

			RegenerateTZXSource( TZX_INSERT, p, 0, &store );
		}

		return CMDOP_REFRESH;

	case 1:
		{
			BYTE BlockID = Selection[ 0 ].Attributes[ 14 ];

			if ( ( BlockID == 0x22 ) || ( BlockID == 0x25 ) || ( BlockID == 0x27 ) || ( BlockID == 0x2A ) || ( BlockID == 0x5A ) )
			{
				MessageBox( hParentWnd,
					L"The selected block is not editable.\r\nTo change the block type, delete the block and create a new one in its place.",
					L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

				return 0;
			}

			if ( ( BlockID == 0x15 ) || ( BlockID == 0x18 ) || ( BlockID == 0x19 ) ) 
			{
				MessageBox( hParentWnd,
					L"The selected block is a complex recording block whose editing is beyond the scope of this application.",
					L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

				return 0;
			}

			if ( Selection[ 0 ].FSFileType != FT_TZX )
			{
				MessageBox( hParentWnd,
					L"The selected item is not a TZX block (it is a file resolved from one or more other blocks), and cannot be edited.\r\n"
					L"If you really want to edit this block, disable file resolution from the context menu, and edit the relevant raw TZX block.",
					L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

				return 0;
			}

			bool Resolve = Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true );

			if ( Resolve )
			{
				if ( ( BlockID == 0x23 ) || ( BlockID == 0x26 ) || ( BlockID == 0x28 ) )
				{
					MessageBox( hParentWnd,
						L"The block cannot be edited correctly while resolved files is enabled, as the block editor is "
						L"unable to determine the correct block offsets. To edit this block, disable Resolve Files first.",
						L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

					return 0;
				}
			}

			CTempFile store;

			ReadBlock( Selection[ 0 ].fileID, store );

			if ( TZXConfigureBlock( hParentWnd, Selection[0].fileID, BlockID, &store, false, true, pDirectory->Files ) == 0 )
			{
				RegenerateTZXSource( TZX_REPLACE, Selection[ 0 ].fileID, 0, &store );

				return CMDOP_REFRESH;
			}
		}

		return 0;

	case 3:
		{
			SPFIX_Warning = false;

			/* Copy the ids to a seperate vector so we can process them while we make modifications */
			std::vector<DWORD> ids;

			for ( NativeFileIterator iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
			{
				ids.push_back( iFile->fileID );
			}

			/* Run each fix separately. Increment the ids each time as we create new blocks */
			DWORD Offset = 0;

			for ( std::vector<DWORD>::iterator id=ids.begin(); id!=ids.end(); id++ )
			{
				SpeedlockBitFix( *id + Offset );

				Offset++;

				pDirectory->ReadDirectory();
			}
		}
		return CMDOP_REFRESH;

	case 8:
		TZXExportOptions( hParentWnd, tzxpb.RegistryPrefix );

		break;

	case 9:
		{
			bool Resolve = Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true );

			Resolve = !Resolve;

			Preference( tzxpb.RegistryPrefix + L"ResolveFiles" ) = Resolve;
		}

		return CMDOP_REFRESH;
	}

	return 0;
}

int TZXFileSystem::ReadTextBlock( NativeFile *pFile, CTempFile &store )
{
	DWORD Offset = pFile->Attributes[ 15 ];

	/* First byte is block ID */
	BYTE Buffer[ 1024 ];

	if ( pSource->ReadRaw( Offset, min( 1024, pSource->PhysicalDiskSize - Offset ), Buffer ) != DS_SUCCESS )
	{
		return -1;
	}

	BYTE c;
	BYTE crlf[2] = { 0x0D, 0x0A };

	store.Seek( 0 );

	DWORD ExOff = 0;

	switch ( pFile->Attributes[ 14 ] )
	{
	case 0x31: // Message
		{
			ExOff++;

			BYTE t = Buffer[ 1 ];

			BYTE tex[ 32 ];
			rsprintf( tex, "[%d secs]: ", (int) t );

			store.Write( tex, rstrnlen( tex, 32 ) );
		}
	case 0x30: // Description
		{
			BYTE l = Buffer[ 1 + ExOff ];

			store.Write( &Buffer[ 2 + ExOff ], l );
			store.Write( &crlf, 2 );
		}
		return 0;

	case 0x32: // Archive Info
		{
			BYTE tex = Buffer[ 3 ]; /* Number of TEXT blocks */

			BYTE *pTex = &Buffer[ 4 ];
			BYTE TexOut[ 32 ];

			for ( c=0; c<tex; c++ )
			{
				BYTE TexType = pTex[ 0 ];
				BYTE lTex    = pTex[ 1 ];

				switch ( TexType )
				{
				case 0x00: rsprintf( TexOut, "Title: " );     break;
				case 0x01: rsprintf( TexOut, "Publisher: " ); break;
				case 0x02: rsprintf( TexOut, "Author(s): " ); break;
				case 0x03: rsprintf( TexOut, "Published: " ); break;
				case 0x04: rsprintf( TexOut, "Language: " );  break;
				case 0x05: rsprintf( TexOut, "Type: " );      break;
				case 0x06: rsprintf( TexOut, "Price: " );     break;
				case 0x07: rsprintf( TexOut, "Protection System: " ); break;
				case 0x08: rsprintf( TexOut, "Origin: " );    break;
				case 0xFF: rsprintf( TexOut, "Comments: " );  break;
				}

				store.Write( TexOut, rstrnlen( TexOut, 32 ) );
				store.Write( &pTex[ 2 ], lTex );
				store.Write( crlf, 2 );

				pTex += lTex + 2;
			}
		}
	}

	return 0;
}

int TZXFileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	BYTE Glue[] = { "ZXTape!\x1A\x01\x01" };

	pSource->WriteRaw( 0, 10, Glue );

	PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

	return 0;
}


void TZXFileSystem::RegenerateTZXSource( BYTE TZXOp, DWORD Pos1, DWORD Pos2, CTempFile *src )
{
	CTempFile Output;

	BYTE Glue[] = { "ZXTape!\x1A\x01\x01" };

	Output.Write( Glue, 10 );

	DWORD FileID = 0;

	NativeFileIterator iFile = pDirectory->Files.begin();

	while ( iFile != pDirectory->Files.end() )
	{
		if ( ( FileID == Pos1 ) && ( TZXOp == TZX_SWAP ) )
		{
			CopyTZX( Output, &pDirectory->Files[ Pos2 ] );
		}
		else if ( ( FileID == Pos2 ) && ( TZXOp == TZX_SWAP ) )
		{
			CopyTZX( Output, &pDirectory->Files[ Pos1 ] );
		}
		else if ( ( FileID == Pos1 ) && ( TZXOp == TZX_DELETE ) )
		{
			FileID++;

			iFile++;

			continue;
		}
		else if ( ( FileID == Pos1 ) && ( TZXOp == TZX_REPLACE ) )
		{
			CopyTZX( Output, src );
		}
		else if ( ( FileID == Pos1 ) && ( TZXOp == TZX_INSERT ) )
		{
			CopyTZX( Output, src );

			CopyTZX( Output, &*iFile );
		}
		else
		{
			CopyTZX( Output, &*iFile );
		}

		FileID++;

		iFile++;
	}

	if ( ( FileID == Pos1 ) && ( TZXOp == TZX_INSERT ) )
	{
		CopyTZX( Output, src );
	}

	/* Adjust jump vectors according to operation */
	ProcessWrittenTZXBlock( Output, Pos1, Pos2, TZXOp);

	ReplaceSourceContent( pSource, Output );
}


void TZXFileSystem::CopyTZX( CTempFile &Output, CTempFile *Source )
{
	DWORD BytesToGo = Source->Ext();

	Source->Seek( 0 );

	while ( BytesToGo > 0 )
	{
		BYTE Buffer[ 16384 ];

		DWORD BytesCopy = min( 16384, BytesToGo );

		Source->Read( Buffer, BytesCopy );
		Output.Write( Buffer, BytesCopy );

		BytesToGo -= BytesCopy;
	}
}

void TZXFileSystem::CopyTZX( CTempFile &Output, NativeFile *pBlockSource )
{
	if ( pBlockSource->FSFileType != FT_TZX )
	{
		RequestResolvedFileWrite( Output, pBlockSource );

		return;
	}

	CTempFile SourceBlock;

	ReadBlock( pBlockSource->fileID, SourceBlock );

	CopyTZX( Output, &SourceBlock );
}

void TZXFileSystem::ProcessWrittenTZXBlock( CTempFile &BlockStore, DWORD Index1, DWORD Index2, int BlockOp )
{
	/* Skip the signature bytes at the beginning */
	QWORD FPos = 0x0A;
	QWORD FLen = BlockStore.Ext();

	BYTE Buffer[ 256 ];

	DWORD CIndex = 0;
	while ( FPos < FLen )
	{
		BlockStore.Seek( FPos );

		/* Read the block ID */
		BlockStore.Read( Buffer, 32 );

		/* Skip the block in most cases */
		switch ( Buffer[ 0 ] )
		{
			case 0x10: FPos += 0x05 + * (WORD *) &Buffer[ 0x03 ]; break;
			case 0x11: FPos += 0x13 + ( ( * (DWORD *) &Buffer[ 0x10 ] ) & 0xFFFFFF ); break;
			case 0x12: FPos += 0x05; break;
			case 0x13: FPos += 0x02 + ( 2 * Buffer[ 0x02 ] ); break;
			case 0x14: FPos += 0x0B + ( ( * (DWORD *) &Buffer[ 0x08 ] ) & 0xFFFFFF ); break;
			case 0x15: FPos += 0x09 + ( ( * (DWORD *) &Buffer[ 0x06 ] ) & 0xFFFFFF ); break;
			case 0x18: case 0x19: FPos += 0x05 + * (DWORD *) &Buffer[ 0x01 ]; break;
			case 0x20: case 0x24: FPos += 0x03; break;
			case 0x21: case 0x30: FPos += 0x02 + Buffer[ 0x01 ]; break;
			case 0x22: case 0x25: case 0x27: FPos += 0x01; break;
			case 0x2A: case 0x2B: FPos += 0x05 + * (DWORD *) &Buffer[ 0x01 ]; break;
			case 0x31: FPos += 0x03 + Buffer[ 0x02 ]; break;
			case 0x32: FPos += 0x03 + * (WORD *) &Buffer[ 0x01 ]; break;
			case 0x33: FPos += 0x02 + ( 3 * Buffer[ 0x01 ] ); break;
			case 0x35: FPos += 0x0F + * (DWORD *) &Buffer[ 0x11 ]; break;
			case 0x5A: FPos += 10;

			case 0x23:
				{
					WORD JumpVec = * (WORD *) &Buffer[ 0x01 ];

					JumpVec = AdjustTZXJumpVector( JumpVec, CIndex, Index1, Index2, BlockOp );

					BlockStore.Seek( FPos + 1 );

					BlockStore.Write( &JumpVec, 2 );

					FPos += 0x03;
				}
				break;

			case 0x26:
				{
					WORD NumVecs = * (WORD *) &Buffer[ 0x01 ];

					WORD JumpVec;

					for ( WORD Vec = 0; Vec<NumVecs; Vec++ )
					{
						BlockStore.Seek( FPos + 0x03 + ( Vec * 0x02 ) );

						BlockStore.Read( &JumpVec, 2 );

						JumpVec = AdjustTZXJumpVector( JumpVec, CIndex, Index1, Index2, BlockOp );

						BlockStore.Seek( FPos + 0x03 + ( Vec * 0x02 ) );

						BlockStore.Write( &JumpVec, 2 );
					}

					FPos += 0x03 + 2 * NumVecs;
				}
				break;

			case 0x28:
				{
					WORD NumVecs = (WORD) Buffer[ 0x03 ];

					WORD JumpVec;

					QWORD SPos = FPos + 0x04;

					for ( WORD Vec = 0; Vec<NumVecs; Vec++ )
					{
						BlockStore.Seek( SPos );

						BlockStore.Read( &JumpVec, 2 );

						BYTE SLen;

						BlockStore.Read( &SLen, 1 );

						JumpVec = AdjustTZXJumpVector( JumpVec, CIndex, Index1, Index2, BlockOp );

						BlockStore.Seek( SPos );

						BlockStore.Write( &JumpVec, 2 );

						SPos += 3 + SLen;
					}

					FPos = SPos;
				}
				break;

			default:
				/* Implement extension rule */
				FPos += 0x05 + * (DWORD *) &Buffer[ 0x01 ];
				break;
		}

		CIndex++;
	}
}

WORD TZXFileSystem::AdjustTZXJumpVector( WORD JumpVec, DWORD CIndex, DWORD Index1, DWORD Index2, int BlockOp )
{
	WORD Vec = JumpVec;

	/* There are special considerations to be taken into account here if the
	   Block being shunted *IS* the jump vector. If we're deleting it, we
	   don't care, but if we're inserting, then we need to correct it's indices
	   accordingly.

	   If we're swapping blocks, then we need to accomodate that, in addition
	   to the vectors shifting anyway. Note: When this function is called, the
	   swap has already happened.
	*/

	/* Convert relative to absolute */
	if ( ( BlockOp != TZX_INSERT ) && ( BlockOp != TZX_REPLACE ) )
	{
		Vec = CIndex + JumpVec;
	}

	if ( ( BlockOp == TZX_INSERT ) && ( Vec > Index1 ) ) { Vec++; }
	if ( ( BlockOp == TZX_DELETE ) && ( Vec >= Index1 ) ) { Vec--; }

	if ( BlockOp == TZX_SWAP ) {
		if ( ( CIndex == Index1 ) && ( Index1 < Index2 ) )
		{
			Vec++;
		}
		else if ( ( CIndex == Index1 ) && ( Index2 < Index1 ) )
		{
			Vec--;
		}

		if ( Vec == Index1 )
		{
			Vec = Index2;
		}
		else if ( Vec == Index2 )
		{
			Vec = Index1;
		}
	}

	/* Convert back to relative */
	Vec = Vec - CIndex;

	return Vec;
}

int TZXFileSystem::SwapFile( DWORD FileID1, DWORD FileID2 )
{
	RegenerateTZXSource( TZX_SWAP, FileID1, FileID2, nullptr );

	return pDirectory->ReadDirectory();
}

int TZXFileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pFile->FSFileType == FT_TZX )
	{
		MessageBox( hMainWindow,
			L"The file object selected is a raw TZX block and has no name to change. To change a resolved file's name, enable Resolved Files in the TZX menu.",
			L"NUTS TZX File System", MB_ICONEXCLAMATION | MB_OK );
	}
	else
	{
		RenameResolvedFile( pFile, NewName, NewExt  );
	}

	return 0;
}

int TZXFileSystem::DeleteFile( DWORD FileID )
{
	RegenerateTZXSource( TZX_DELETE, FileID, 0, nullptr );

	return pDirectory->ReadDirectory();
}

BYTE *TZXFileSystem::GetTitleString( NativeFile *pFile )
{
	static const BYTE *Title = (const BYTE *) "TZX Image";

	return (BYTE *) Title;
}

BYTE *TZXFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE Descript[ 256 ];

	GetBlockDescription( Descript, FileIndex );

	return Descript;
}

BYTE *TZXFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE Status[ 256 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( Status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( Status, "%d Objects Selected", SelectedItems );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[ FileIndex ];

		BYTE Descript[ 128 ];

		GetBlockDescription( Descript, FileIndex );

		if ( pFile->Flags & FF_Extension )
		{
			rsprintf( Status, "%s.%s: %s", (BYTE *) pFile->Filename, pFile->Extension, Descript );
		}
		else
		{
			rsprintf( Status, "%s: %s", (BYTE *) pFile->Filename, Descript );
		}
	}

	return Status;
}

#define BLOCKBYTE(x)  ( Block[x] )
#define BLOCKWORD(x)  ( * (WORD *) &Block[x] )
#define BLOCKDWORD(x) ( * (WORD *) &Block[x] )

void TZXFileSystem::GetBlockDescription( BYTE *Buffer, DWORD FileIndex )
{
	NativeFile *pFile = &pDirectory->Files[ FileIndex ];

	if ( pFile->FSFileType != FT_TZX )
	{
		DescribeResolvedFile( Buffer, pFile );

		return;
	}

	BYTE BlockID = pFile->Attributes[ 14 ];
	DWORD Offset = pFile->Attributes[ 15 ];

	/* Read 64 bytes in, which should cover any block */
	BYTE Block[ 64 ];

	pSource->ReadRaw( Offset + 1, 64, Block );

	DWORD Length = pFile->Length;

	switch (BlockID)
	{
	case 0x10:
		rsprintf( Buffer, "%d bytes, %d pause after", Length, BLOCKWORD( 0 ) );
		break;
	case 0x11:
		rsprintf( Buffer, "%d bytes, %d/%d/%d/%d/%d/%d/%d/%d pause after", Length,
			BLOCKWORD(0), BLOCKWORD(0x0A), BLOCKWORD(2), BLOCKWORD(4), BLOCKWORD(6), BLOCKWORD(8), BLOCKBYTE(12), BLOCKWORD(13) );
		break;
	case 0x12:
		rsprintf( Buffer, "%d/%d", BLOCKWORD( 0 ), BLOCKWORD(2) );
		break;
	case 0x13:
		rsprintf( Buffer, "%d pulses", BLOCKBYTE( 0 ) );
		break;
	case 0x14:
		rsprintf( Buffer, "%d bytes, %d/%d/%d, %d pause after", Length, BLOCKWORD( 0 ), BLOCKWORD(2), BLOCKBYTE(4), BLOCKWORD(5) );
		break;
	case 0x15:
	case 0x18:
	case 0x19:
		rsprintf( Buffer, "Unsupported" );
		break;
	case 0x20:
		rsprintf( Buffer, "%d milliseconds", BLOCKWORD( 0 ) );
		break;
	case 0x21:
		{
			BYTE str[ 256 ];

			ConvertBlockString( &Block[ 0 ], str );

			rsprintf( Buffer, "%s", str );
		}
		break;
	case 0x22:
		rsprintf( Buffer, "" );
		break;
	case 0x23:
		{
			unsigned short x = BLOCKWORD( 0 );

			WORD Target = pFile->fileID; Target += x;

			rsprintf( Buffer, "%d", Target );
		}
		break;
	case 0x24:
		rsprintf( Buffer, "%d repititions", BLOCKWORD( 0 ) );
		break;
	case 0x25:
		rsprintf( Buffer, "" );
		break;
	case 0x26:
		rsprintf( Buffer, "%d calls", BLOCKWORD( 0 ) );
		break;
	case 0x27:
		rsprintf( Buffer, "" );
		break;
	case 0x28:
		rsprintf( Buffer, "%d sections", BLOCKBYTE( 2 ) );
		break;
	case 0x2A:
		rsprintf( Buffer, "" );
		break;
	case 0x2B:
		rsprintf( Buffer, "%s", ( (BLOCKBYTE(4)==0)?"Low":"High") );
		break;
	case 0x30:
		{
			BYTE str[ 256 ];

			ConvertBlockString( &Block[ 0 ], str );

			rsprintf( Buffer, "%s", str );
		}
		break;
	case 0x31:
		{
			BYTE str[ 256 ];

			ConvertBlockString( &Block[ 1 ], str );

			rsprintf( Buffer, "%d secs: %s", BLOCKBYTE( 0 ), str );
		}
		break;
	case 0x32:
		rsprintf( Buffer, "%d fields", BLOCKBYTE( 2 ) );
		break;
	case 0x33:
		rsprintf( Buffer, "%d fields", BLOCKBYTE( 0 ) );
		break;
	case 0x35:
		rsprintf( Buffer, "%d bytes", Length - 0x14 );
		break;
	case 0x5A:
		rsprintf( Buffer, "" );
		break;
	case 0x4B:
		rsprintf( Buffer, "%d bytes, %d/%d/%d/%d/%d pause after", Length - 0x10,
			BLOCKWORD(6), BLOCKWORD(8), BLOCKWORD(10), BLOCKWORD(12), BLOCKWORD(4) );
		break;
	}
}

void TZXFileSystem::ConvertBlockString( BYTE *ptr, BYTE *out )
{
	BYTE l = *ptr;

	ptr++;

	ZeroMemory( out, l + 1 );

	rstrncpy( out, ptr, l );
}

void TZXFileSystem::RequestResolvedFileWrite( CTempFile &Output, NativeFile *pFile )
{
	return;
}

void TZXFileSystem::DescribeResolvedFile( BYTE *Buffer, NativeFile *pFile )
{
	rsprintf( Buffer, "Resolved file" );
}

void TZXFileSystem::RenameResolvedFile( NativeFile *pFile, BYTE *NewName, BYTE *NewExt  )
{
	pDirectory->ReadDirectory();

	return;
}

void TZXFileSystem::MakeResolvedAudio( NativeFile *pFile, CTempFile &output, TapeIndex &indexes )
{
}

const WCHAR *TZXFileSystem::DescribeBlock( BYTE BlockID )
{
	BYTE i = 0 ;

	while ( TZXIDs[ i ] != 0x00 )
	{
		if ( TZXIDs[ i ] == BlockID )
		{
			return TZXBlockTypes[ i ];
		}

		i++;
	}

	return nullptr;
}

WCHAR *TZXFileSystem::Identify( DWORD FileID )
{
	if ( pDirectory->Files[ FileID ].FSFileType == FT_TZX )
	{
		static WCHAR Identifier[ 256 ];

		swprintf_s( Identifier, 255, L"TZX %s", DescribeBlock( pDirectory->Files[ FileID ].Attributes[ 14 ] ) );

		return Identifier;
	}

	return FileSystem::Identify( FileID );
}

int TZXFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( pFile->FSFileType != FT_TZX )
	{
		if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != tzxpb.Encoding ) )
		{
			return FILEOP_NEEDS_ASCII;
		}

		CTempFile ExBlocks;

		/* Import as std/turbo/pure */
		DWORD ImportOpt = Preference( tzxpb.RegistryPrefix + L"_ImportOption", (DWORD) 0 );

		BYTE BlockID;

		switch ( ImportOpt )
		{
		case 0: /* Standard - make it bytes @ 0x400 */
			{
				BYTE Header[ 19 ];
				ZeroMemory( Header, 19 );

				Header[ 1 ] = 3;

				rstrncpy( &Header[ 2 ], pFile->Filename, 10 );

				for ( BYTE i=9; i>0; i-- ) { if ( Header[ 2 + i ] == 0 ) { Header[ 2 + i ] = 0x20; } else { break; } }

				WORD DataLen = (WORD) pFile->Length;
				WORD Pause   = 1000;

				BlockID = 0x10;

				* (WORD *) &Header[ 12 ] = DataLen;
				* (WORD *) &Header[ 14 ] = 16384;
				* (WORD *) &Header[ 16 ] = 32768;

				for ( BYTE i=0; i<18; i++ ) { Header[ 18 ] ^= Header[ i ]; }

				DataLen += 2;

				WORD HeadLen = 19;
				BYTE Flag    = 0xFF;

				ExBlocks.Seek( 0 );
				ExBlocks.Write( &BlockID, 1 );
				ExBlocks.Write( &Pause,   2 );
				ExBlocks.Write( &HeadLen, 2 );
				ExBlocks.Write( Header, 19 );

				Pause = 2000;

				ExBlocks.Write( &BlockID, 1 );
				ExBlocks.Write( &Pause,   2 );
				ExBlocks.Write( &DataLen, 2 );
				ExBlocks.Write( &Flag,    1 );
			}
			break;

		case 1: /* Turbo data - use 146% values */
			{
				BlockID = 0x11;

				WORD Pulse;
				BYTE Bits  = 0;
				WORD Pause = 2000;

				ExBlocks.Write( &BlockID, 1 );

				Pulse = 2168; ExBlocks.Write( &Pulse, 2 );
				Pulse = 667;  ExBlocks.Write( &Pulse, 2 );
				Pulse = 735;  ExBlocks.Write( &Pulse, 2 );
				Pulse = 585;  ExBlocks.Write( &Pulse, 2 );
				Pulse = 1170; ExBlocks.Write( &Pulse, 2 );
				Pulse = 3223; ExBlocks.Write( &Pulse, 2 );

				ExBlocks.Write( &Bits,  1 );
				ExBlocks.Write( &Pause, 2 );

				DWORD DataLen = (DWORD) pFile->Length;

				ExBlocks.Write( &DataLen, 3 );
			}
			break;

		case 2: /* Pure data - use 146% values */
			{
				BlockID = 0x14;

				WORD Pulse;
				BYTE Bits  = 0;
				WORD Pause = 2000;

				ExBlocks.Write( &BlockID, 1 );

				Pulse = 585;  ExBlocks.Write( &Pulse, 2 );
				Pulse = 1170; ExBlocks.Write( &Pulse, 2 );

				ExBlocks.Write( &Bits,  1 );
				ExBlocks.Write( &Pause, 2 );

				DWORD DataLen = (DWORD) pFile->Length;

				ExBlocks.Write( &DataLen, 3 );
			}
			break;
		}

		BYTE ck = 0xFF;

		/* Copy the data */
		DWORD BytesToGo = pFile->Length;
		BYTE  Buffer[ 16384 ];

		store.Seek( 0 );

		while ( BytesToGo > 0 )
		{
			DWORD BytesRead = min( 16384, BytesToGo );

			store.Read( Buffer, BytesRead );

			for ( WORD i=0; i<BytesRead; i++ ) { ck ^= Buffer[ i ]; }

			ExBlocks.Write( Buffer, BytesRead );

			BytesToGo -= BytesRead;
		}

		if ( ImportOpt == 0 ) { ExBlocks.Write( &ck, 1 ); }

		RegenerateTZXSource( TZX_INSERT, pDirectory->Files.size(), 0, &ExBlocks );
	}
	else
	{
		/* Import as is (we assume the exporting file system is not exporting subsets) */
		RegenerateTZXSource( TZX_INSERT, pDirectory->Files.size(), 0, &store );
	}

	return pDirectory->ReadDirectory();
}

void TZXFileSystem::SpeedlockBitFix( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	/* If this isn't a pure data block, blow raspberries */
	if ( pFile->Attributes[ 14 ] != 0x14 )
	{
		MessageBox( hPaneWindow, L"The Speedlock Bit Fix cannot be applied to a block other than Pure Data blocks.", L"NUTS TZX Configuration", MB_ICONINFORMATION | MB_OK );

		return;
	}

	if ( !SPFIX_Warning )
	{
		/* Check it with the user, as this will make the file weird */
		if ( MessageBox( hPaneWindow,
			L"Some versions of the Speedlock protection system don't use encryption, but the TZX recording tools "
			L"erroneously misalign bits, causing data extracted from pure data blocks to be misaligned.\r\n\r\n"
			L"This function can attempt to fix this by splitting a pure data block into two, with the first "
			L"containing a single bit. This will not affect playback, but will cause an additional block to be "
			L"added to the image. This operation may need to be repeated several times to restore alignment.\r\n\r\n"
			L"Continue?", L"NUTS TZX Configuration", MB_ICONQUESTION | MB_YESNO ) == IDNO ) { return; }

		SPFIX_Warning = true;
	}

	/* Create two block files to hold the data */
	CTempFile NewBlock;
	CTempFile ShiftBlock;

	BYTE Buffer[ 0x4001 ];
	
	DWORD Offset = pFile->Attributes[ 15 ];

	pSource->ReadRaw( Offset, 0x0B, Buffer );

	/* The shifted block requires the used bit continue decrementing. If it passes zero
	   (goes to 255), it needs to drop to 7. */

	Buffer[ 0x05 ] = ( Buffer[ 0x05 ] - 1 ) & 0x3;

	/* If it did so, there is one less byte */
	DWORD DataLen = * (DWORD *) &Buffer[ 0x08 ];

	DataLen &= 0xFFFFFF;

	if ( Buffer[ 0x05 ] == 7 ) { DataLen--; }

	ShiftBlock.Write( Buffer, 0x08 );
	ShiftBlock.Write( &DataLen, 3  );

	/* Set up the header for the new block */
	Buffer[ 0x08 ] = 1; /* Only 1 byte */
	Buffer[ 0x09 ] = 0;
	Buffer[ 0x0A ] = 0;
	Buffer[ 0x05 ] = 1; /* Only one bit */
	Buffer[ 0x06 ] = 0; /* No pause! */
	Buffer[ 0x07 ] = 0;

	NewBlock.Write( Buffer, 0x0B );

	/* Now read the first byte of the original block, and insert the first bit into the new block */
	BYTE Byte1;

	Offset += 0x0B;

	pSource->ReadRaw( Offset, 1, &Byte1 );

	Byte1 &= 0x80;

	NewBlock.Write( &Byte1, 1 );

	/* Now read the whole file. Since we need to shift bits, we'll have to do this one byte at a time,
	   but read two bytes together. */

	WORD  Data;
	DWORD BytesToGo = DataLen;

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 0x4000, BytesToGo );

		pSource->ReadRaw( Offset + 0, BytesRead + 1, Buffer );

		for ( WORD i=0; i<0x4000; i++ )
		{
			Buffer[ i ] = ( Buffer[ i ] << 1 ) | ( ( Buffer[ i + 1 ] & 0x80 ) >> 7 );
		}

		ShiftBlock.Write( &Buffer, BytesRead );

		BytesToGo -= BytesRead;
		Offset    += BytesRead;
	}

	/* Now replace the original block with the shift block.... */
	RegenerateTZXSource( TZX_REPLACE, FileID, 0, &ShiftBlock );

	/* And insert the new block */
	RegenerateTZXSource( TZX_INSERT, FileID, 0, &NewBlock );
}

#define TStates(t) ( ( (double) t / 3500000.0 ) * 44100.0 )
#define PStates(t) ( ( (double) t / 1000.0 ) * 44100.0 )

void TZXFileSystem::MakeAudioPulses( DWORD PulseWidth, DWORD PulseCycles, CTempFile *output )
{
	BYTE sig;

	for ( DWORD i=0; i<PulseCycles; i++ )
	{
		double ss = TStates( PulseWidth );
		
		DWORD ssr = (DWORD) round(ss);

		for ( DWORD s = 0; s<ssr; s++ )
		{
			sig = 127 + (127 * SignalLevel );

			TapeAudio[ TapePtr++ ] = sig;

			AudioPtr++;

			WriteAudio( output );
		}

		SignalLevel = 1 - SignalLevel;
	}
}

void TZXFileSystem::MakeAudioPulse( DWORD PulseWidth1, DWORD PulseWidth2, CTempFile *output )
{
	BYTE sig;

	double ss = TStates( PulseWidth1 );
		
	DWORD ssr = (DWORD) round(ss);

	sig = 127 + (127 * SignalLevel );

	for ( DWORD s = 0; s<ssr; s++ )
	{
		TapeAudio[ TapePtr++ ] = sig;

		AudioPtr++;

		WriteAudio( output );
	}

	SignalLevel = 1 - SignalLevel;

	if ( PulseWidth2 == 0 ) { return; }

	ss = TStates( PulseWidth2 );
		
	ssr = (DWORD) round(ss);

	sig = 127 + (127 * SignalLevel );

	for ( DWORD s = 0; s<ssr; s++ )
	{
		TapeAudio[ TapePtr++ ] = sig;

		AudioPtr++;

		WriteAudio( output );
	}

	SignalLevel = 1 - SignalLevel;
}

void TZXFileSystem::MakeAudioBytes( BYTE *pData, DWORD lData, CTempFile *output )
{
	for ( DWORD p=0; p<lData; p++ )
	{
		for (BYTE b=128; b!=0; b>>=1 )
		{
			if ( ( pData[ p ] & b ) == b )
			{
				MakeAudioPulse( 1710, 1710, output );
			}
			else
			{
				MakeAudioPulse( 855, 855, output );
			}
		}
	}
}

void TZXFileSystem::MakeAudioBytes( BYTE *pData, DWORD lData, WORD l1, WORD l2, BYTE br, bool LastBlock, CTempFile *output )
{
	for ( DWORD p=0; p<lData; p++ )
	{
		BYTE lb = 0;

		if ( ( br != 0 ) && ( p == ( lData - 1 ) ) && ( LastBlock ) )
		{
			lb = ( 1 << ( 7 - br ) );
		}

		for (BYTE b=128; b!=lb; b>>=1 )
		{
			if ( ( pData[ p ] & b ) == b )
			{
				MakeAudioPulse( l2, l2, output );
			}
			else
			{
				MakeAudioPulse( l1, l1, output );
			}
		}
	}
}

void TZXFileSystem::MakePause( DWORD ms, CTempFile *output )
{
	BYTE sig = 127;

	DWORD ss = PStates( ms );

	for ( DWORD s = 0; s<ss; s++ )
	{
		TapeAudio[ TapePtr++ ] = sig;

		AudioPtr++;

		WriteAudio( output );
	}
}

void TZXFileSystem::WriteAudio( CTempFile *output )
{
	if ( TapePtr >= TapeLimit )
	{
		output->Write( TapeAudio, TapePtr );

		TapePtr = 0;
	}
}

int TZXFileSystem::MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store )
{
	SignalLevel = 0;
	TapePtr     = 0;
	AudioPtr    = 0;
	TapeLimit   = 16384;
	TapePartial = 0.0;

	NativeFileIterator iFile;

	store.Seek( 0 );

	WORD LoopCount = 0;
	WORD LoopBlock = 0;
	bool Looping   = false;

	std::map<DWORD, bool> JumpBlocks;

	indexes.Title = L"TZX Image";

	NativeFileIterator CallSource;
	WORD               CallIndex;
	bool               CallSequence = false;

	for ( iFile = Selection.begin(); iFile != Selection.end();  )
	{
		bool IncIt = true;
		bool CueIt = true;

		if ( iFile->FSFileType != FT_TZX )
		{
			MakeResolvedAudio( &*iFile, store, indexes );

			iFile++;

			continue;
		}

		BYTE TZXHeader[ 0x200 ];

		DWORD bl = pDir->BlockLengths[ iFile->fileID ];

		pSource->ReadRaw( iFile->Attributes[ 15 ], 0x200, TZXHeader );

		TapeCue Cue;

		Cue.IndexPtr = AudioPtr;

		switch ( iFile->Attributes[ 14 ] )
		{
		case 0x10:
			{
				if ( TZXHeader[ 0x05 ] == 0x00 )
				{
					Cue.IndexName = GetZXFilename( &TZXHeader[ 0x06 ] );
					Cue.Flags     = 0;

					MakeAudioPulses( 2168, 8063, &store );
				}
				else
				{
					Cue.IndexName = L"Data Block, " + std::to_wstring( (QWORD) ( bl - 5 ) ) + L" bytes";
					Cue.Flags     = 0;

					MakeAudioPulses( 2168, 3223, &store );
				}

				MakeAudioPulse( 667, 753, &store );

				DWORD BytesToGo = bl;
				DWORD Offset    = iFile->Attributes[ 15 ] + 0x05;

				BYTE  Buffer[ 16384 ];

				while ( BytesToGo > 0 )
				{
					DWORD BytesRead = min( 16384, BytesToGo );

					pSource->ReadRaw( Offset, BytesRead, Buffer );

					MakeAudioBytes( Buffer, BytesRead, &store );

					Offset    += BytesRead;
					BytesToGo -= BytesRead;
				}

				MakePause( * (WORD *) &TZXHeader[ 0x01 ], &store );
			}
			break;

		case 0x11:
			{
				Cue.IndexName = L"Turbo Data, " + std::to_wstring( (QWORD) ( bl - 0x13 ) ) + L" bytes";
				Cue.Flags     = 0;

				MakeAudioPulses( * (WORD *) &TZXHeader[ 0x01 ], * (WORD *) &TZXHeader[ 0x0B ], &store );
				MakeAudioPulse( * (WORD *) &TZXHeader[ 0x03 ], * (WORD *) &TZXHeader[ 0x05 ], &store );

				DWORD BytesToGo = bl;
				DWORD Offset    = iFile->Attributes[ 15 ] + 0x13;

				BYTE  Buffer[ 16384 ];

				while ( BytesToGo > 0 )
				{
					DWORD BytesRead = min( 16384, BytesToGo );

					pSource->ReadRaw( Offset, BytesRead, Buffer );

					MakeAudioBytes( Buffer, BytesRead, * (WORD *) &TZXHeader[ 0x07 ], * (WORD *) &TZXHeader[ 0x09 ], TZXHeader[ 0x0D ], (BytesToGo == BytesRead), &store );

					Offset    += BytesRead;
					BytesToGo -= BytesRead;
				}

				MakePause( * (WORD *) &TZXHeader[ 0x0E ], &store );
			}
			break;

		case 0x12:
			{
				Cue.IndexName = L"Pure Tone";
				Cue.Flags     = 0;

				MakeAudioPulses( * (WORD *) &TZXHeader[ 0x01 ], * (WORD *) &TZXHeader[ 0x03 ], &store );
			}
			break;

		case 0x13:
			{
				Cue.IndexName = L"Pulse Sequence, " + std::to_wstring( (QWORD) TZXHeader[ 0x01 ] ) + L" pulses";
				Cue.Flags     = 0;

				BYTE n = TZXHeader[ 0x01 ];

				for ( BYTE t=0; t<n; t++ )
				{
					MakeAudioPulse( * (WORD *) &TZXHeader[ 0x02 + 2 * t ], 0, &store );
				}
			}
			break;

		case 0x14:
			{
				Cue.IndexName = L"Pure Data, " + std::to_wstring( (QWORD) ( bl - 0x0B ) ) + L" bytes";
				Cue.Flags     = 0;

				DWORD BytesToGo = bl;
				DWORD Offset    = iFile->Attributes[ 15 ] + 0x0B;

				BYTE  Buffer[ 16384 ];

				while ( BytesToGo > 0 )
				{
					DWORD BytesRead = min( 16384, BytesToGo );

					pSource->ReadRaw( Offset, BytesRead, Buffer );

					MakeAudioBytes( Buffer, BytesRead, * (WORD *) &TZXHeader[ 0x01 ], * (WORD *) &TZXHeader[ 0x03 ], TZXHeader[ 0x05 ], (BytesToGo == BytesRead), &store );

					Offset    += BytesRead;
					BytesToGo -= BytesRead;
				}

				MakePause( * (WORD *) &TZXHeader[ 0x06 ], &store );
			}
			break;

		case 0x15:
			{
				WORD  SampleRate = * (WORD *) &TZXHeader[ 0x01 ];
				WORD  Pause      = * (WORD *) &TZXHeader[ 0x03 ];
				DWORD DataLen    = * (DWORD *) &TZXHeader[ 0x06 ]; DataLen &= 0xFFFFFF;
				DWORD BytesToGo  = bl;
				DWORD Offset     = iFile->Attributes[ 15 ] + 0x09;
				BYTE  UsedBits   = TZXHeader[ 0x05 ];
				DWORD Samps      = ( DataLen * 8 );

				if ( UsedBits != 0 ) { Samps -= 8 - UsedBits; }

				Cue.IndexName = L"Dir. Recording, " + std::to_wstring( (QWORD) Samps ) + L" samples";
				Cue.Flags     = 0;

				BYTE  Buffer[ 16384 ];

				double ss = TStates( SampleRate );
		
				DWORD ssr = (DWORD) round(ss);

				while ( BytesToGo > 0 )
				{
					DWORD BytesRead = min( 16384, BytesToGo );

					pSource->ReadRaw( Offset, BytesRead, Buffer );

					for ( DWORD p=0; p<BytesRead; p++ )
					{
						BYTE lb = 0;

						if ( ( UsedBits != 0 ) && ( p == ( BytesRead - 1 ) ) && ( BytesToGo == BytesRead ) )
						{
							lb = ( 1 << ( 7 - UsedBits ) );
						}

						for (BYTE b=128; b!=lb; b>>=1 )
						{
							BYTE sig = 127;

							if ( ( Buffer[ p ] & b ) == b )
							{
								sig += 127;
							}

							for ( DWORD s = 0; s<ssr; s++ )
							{
								TapeAudio[ TapePtr++ ] = sig;

								AudioPtr++;

								WriteAudio( &store );
							}
						}
					}

					Offset    += BytesRead;
					BytesToGo -= BytesRead;
				}

				MakePause( * (WORD *) &TZXHeader[ 0x06 ], &store );
			}
			break;

		case 0x20:
			{
				WORD Pause = * (WORD *) &TZXHeader[ 0x01 ];

				if ( Pause == 0 )
				{
					Cue.IndexName = L"Forced Stop";
					Cue.Flags     = 0;

					iFile = Selection.end();

					IncIt = false;
				}
				else
				{
					Cue.IndexName = L"Pause, " + std::to_wstring( (QWORD) Pause ) + L" ms";
					Cue.Flags     = 0;

					MakePause( Pause, &store );
				}
			}
			break;

		case 0x21:
			{
				Cue.IndexName = L"Group start";

				DWORD Offset = iFile->Attributes[ 15 ] + 0x01;
				BYTE  l;
				BYTE  name[256];

				ZeroMemory( name, 256 );
				pSource->ReadRaw( Offset + 0, 1, &l );
				pSource->ReadRaw( Offset + 1, l, name );

				Cue.Extra = std::wstring( UString( (char *) name ) );
				Cue.Flags = TF_ExtraValid | TF_Extra1;
			}
			break;

		case 0x22:
			Cue.IndexName = L"Group End";
			Cue.Extra     = L"";
			Cue.Flags     = TF_ExtraValid | TF_Extra1;
			break;

		case 0x23:
			{
				/* Don't jump from the same block twice - this is an infinite loop */
				if ( JumpBlocks.find( iFile->fileID ) == JumpBlocks.end() )
				{
					Cue.IndexName = L"Jump";
					Cue.Flags     = 0;

					WORD JumpVec = * (WORD *) &TZXHeader[ 0x01 ];

					JumpVec += (WORD) iFile->Attributes[ 13 ];

					JumpBlocks[ iFile->fileID ] = true;

					/* There is an issue here. If the user has not selected the jump target,
						then the chunk won't exist. In that case, we skip it */
					NativeFileIterator iTrg;

					for ( iTrg = Selection.begin(); iTrg != Selection.end(); iTrg++ )
					{
						if ( iTrg->Attributes[ 13 ] == JumpVec )
						{
							iFile = iTrg;

							break;
						}
					}
				}
			}
			break;

		case 0x24:
			if ( !Looping )
			{
				LoopBlock = iFile->fileID;
				LoopCount = * (WORD *) &TZXHeader[ 0x01 ];
				Looping   = true;
			}

			CueIt = false;

			break;

		case 0x25:
			{
				if ( LoopCount > 0 )
				{
					LoopCount--;

					iFile = Selection.begin() + LoopBlock;

					IncIt = false;
				}
				else
				{
					Looping = false;
				}
			}

			CueIt = false;

			break;

		case 0x26:
			{
				Cue.IndexName = L"Call Sequence";
				Cue.Flags     = 0;

				DWORD Offset    = iFile->Attributes[ 15 ] + 0x01;

				/* Don't allow nested calls */
				if ( ( CallSequence ) && ( CallSource != iFile ) )
				{
					break;
				}

				if ( !CallSequence )
				{
					CallIndex  = 0;
					CallSource = iFile;
				}
				else
				{
					CallIndex++;
				}

				WORD Calls;

				pSource->ReadRaw( Offset, 2, (BYTE *) &Calls );

				if ( CallIndex == Calls )
				{
					CallSequence = false;

					break;
				}

				Offset += 2 + ( 2 * CallIndex );

				WORD JumpVec;

				pSource->ReadRaw( Offset, 2, (BYTE *) &JumpVec );

				JumpVec += (WORD) iFile->Attributes[ 13 ];

				/* There is an issue here. If the user has not selected the jump target,
					then the chunk won't exist. In that case, we skip it */
				NativeFileIterator iTrg;

				for ( iTrg = Selection.begin(); iTrg != Selection.end(); iTrg++ )
				{
					if ( iTrg->Attributes[ 13 ] == JumpVec )
					{
						CallSource = iFile;

						iFile = iTrg;

						CallSequence = true;

						break;
					}
				}
			}
			break;

		case 0x27:
			{
				Cue.IndexName = L"Call Return";
				Cue.Flags     = 0;

				if ( CallSequence )
				{
					iFile = CallSource;

					IncIt = false;
				}
			}
			break;

		case 0x28:
			{
				/* Don't do the same select block twice */
				if ( JumpBlocks.find( iFile->fileID ) != JumpBlocks.end() )
				{
					CueIt = false;

					break;
				}

				JumpBlocks[ iFile->fileID ] = true;

				std::vector<std::wstring> Paths;
				std::vector<WORD> Offsets;

				BYTE SelHdr[ 3 ];
				BYTE SelPath[ 256 ];

				QWORD Offset    = iFile->Attributes[ 15 ] + 0x01;

				pSource->ReadRaw( Offset, 3, SelHdr );

				BYTE NumPaths = SelHdr[ 2 ];

				Offset += 3;

				for ( BYTE i=0; i<NumPaths; i++ )
				{
					memset( SelPath, 0, 256 );

					pSource->ReadRaw( Offset, 3, SelHdr );

					BYTE TLen = SelHdr[ 2 ];
					WORD Rel  = * (WORD *) &SelHdr[ 0 ];

					pSource->ReadRaw( Offset + 3, TLen, SelPath );

					Paths.push_back( std::wstring( UString( (char *) SelPath ) ) );

					Rel += (WORD) iFile->Attributes[ 13 ];

					Offsets.push_back( Rel );

					Offset += 3 + TLen;
				}

				int PathSelected = TZXSelectPath( NULL, Paths );

				if ( PathSelected != -1 )
				{
					WORD Rel = Offsets[ PathSelected ];

					/* There is an issue here. If the user has not selected the jump target,
						then the chunk won't exist. In that case, we skip it */
					NativeFileIterator iTrg;

					for ( iTrg = Selection.begin(); iTrg != Selection.end(); iTrg++ )
					{
						if ( iTrg->Attributes[ 13 ] == Rel )
						{
							iFile = iTrg;

							IncIt = false;
							CueIt = false;

							break;
						}
					}
				}
				else
				{
					iFile = Selection.end();

					IncIt = false;
					CueIt = false;
				}
			}
			break;

		case 0x2A:
			Cue.IndexName = L"48K Stop";
			Cue.Flags     = 0;
			break;

		case 0x2b:
			Cue.IndexName = L"Set signal level";
			Cue.Flags     = 0;

			SignalLevel = TZXHeader[ 0x05 ] & 1;
			break;
			
		case 0x30:
			{
				Cue.IndexName = L"Description";
				Cue.Flags     = TF_ExtraValid | TF_Extra2;

				BYTE l;
				BYTE t[256];

				ZeroMemory( t, 256 );
				DWORD Offset = iFile->Attributes[ 15 ] + 0x01;

				pSource->ReadRaw( Offset + 0, 1, &l );
				pSource->ReadRaw( Offset + 1, l, t );

				Cue.Extra = std::wstring( UString( (char *) t ) );
			}
			break;

		case 0x31:
			{
				Cue.IndexName = L"Message";
				Cue.Flags     = TF_ExtraValid | TF_Extra3;

				BYTE l;
				BYTE t[256];

				ZeroMemory( t, 256 );
				DWORD Offset = iFile->Attributes[ 15 ] + 0x02;

				pSource->ReadRaw( Offset + 0, 1, &l );
				pSource->ReadRaw( Offset + 1, l, t );

				Cue.Extra = std::wstring( UString( (char *) t ) );

				MakePause( TZXHeader[ 01 ] * 1000, &store );
			}
			break;

		case 0x32:
			Cue.IndexName = L"Archive Info";
			Cue.Flags     = 0;

			{
				DWORD Offset = iFile->Attributes[ 15 ] + 0x04;
				BYTE NumBlks = TZXHeader[ 0x03 ];

				for ( BYTE blk=0; blk<NumBlks; blk++ )
				{
					BYTE tr[256];
					BYTE len;
					BYTE t;

					ZeroMemory( tr, 256 );

					pSource->ReadRaw( Offset + 0, 1, &t );
					pSource->ReadRaw( Offset + 1, 1, &len );
					pSource->ReadRaw( Offset + 2, len, tr );

					if ( t == 00 ) { indexes.Title = std::wstring( UString( (char *) tr ) ); }
					if ( t == 01 ) { indexes.Publisher = std::wstring( UString( (char *) tr ) ); }

					Offset += 2 + len;
				}
			}
		}

		if ( CueIt ) { indexes.Cues.push_back( Cue ); }

		if ( IncIt ) { iFile++; }
	}

	if ( TapePtr > 0 )
	{
		TapeLimit = TapePtr;

		WriteAudio( &store );
	}

	return 0;
}

std::wstring TZXFileSystem::GetZXFilename( BYTE *header )
{
	BYTE AHeader[ 11 ];

	AHeader[ 10 ] = 0;

	memcpy( AHeader, &header[1], 10 );

	for ( BYTE i=9; i>0; i-- ) {
		if ( AHeader[ i ] == 0x20 )
		{
			AHeader[ i ] = 0;
		}
		else
		{
			break;
		}
	}

	std::wstring pt;

	switch ( header[ 0 ] )
	{
	case 0:
		pt = L"Program";
		break;

	case 1:
		pt = L"Character Array";
		break;

	case 2:
		pt = L"Number Array";
		break;

	case 3:
		pt = L"Bytes";
		break;

	default:
		pt = L"Unknown";
	}

	return pt + L": " + std::wstring( UString( (char *) AHeader ) );
}