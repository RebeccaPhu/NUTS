#include "StdAfx.h"
#include "SinclairTZXFileSystem.h"
#include "SinclairTZXDirectory.h"

#include "../NUTS/Preference.h"
#include "../TZXConfig/TZXHardware.h"
#include "../TZXConfig/TZXConfig.h"
#include "resource.h"

int SinclairTZXFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	BYTE Buffer[ 0x4000 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pFile->FSFileType != FT_SINCLAIR )
	{
		return TZXFileSystem::ReadFile( FileID, store );
	}

	DWORD BytesToGo = pFile->Length;

	QWORD Offset = pFile->Attributes[ 0 ];

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

void SinclairTZXFileSystem::RequestResolvedFileWrite( CTempFile &Output, NativeFile *pFile )
{
	BYTE  Header[ 19 ];

	Header[ 0 ] = 0x00; /* Flag byte */

	/* Fix up the filename with padding */
	Header[ 12 ] = 0;
	memcpy( &Header[ 2 ], (BYTE *) pFile->Filename, 10 );

	BYTE n = rstrnlen( &Header[ 2 ], 10 );

	if ( n != 10 )
	{
		for ( BYTE i=n; i<10; i++ )
		{
			Header[ 2 + i ] = 0x20;
		}
	}

	/* The remaining attributes */
	Header[ 1 ] = pFile->Attributes[ 2 ];

	* (WORD *) &Header[ 0x0C ] = (WORD) pFile->Length;

	switch ( Header[ 1 ] )
	{
	case 0:
		{
			* (WORD *) &Header[ 0x0E ] = (WORD) pFile->Attributes[ 3 ];
			* (WORD *) &Header[ 0x10 ] = (WORD) pFile->Attributes[ 4 ];
		}
		break;

	case 1:
	case 2:
		{
			Header[ 0x0F ] = (BYTE) pFile->Attributes[ 5 ];
		}
		break;

	case 3:
		{
			* (WORD *) &Header[ 0x0E ] = (WORD) pFile->Attributes[ 1 ];
			* (WORD *) &Header[ 0x10 ] = 32768;
		}
		break;
	}

	/* Checksum the header */
	Header[ 18 ] = 0;

	for (BYTE i=0; i<18; i++) { Header[ 18 ] ^= Header[ i ]; }

	BYTE BlkID = 0x10;

	/* Write the header TZX block, but only if it is not an ORPHAN block */
	if ( pFile->Attributes[ 2 ] != 0xFFFFFFFF )
	{
		WORD Pause   = 1000;
		WORD DataLen = 19;

		Output.Write( &BlkID,   1 );
		Output.Write( &Pause,   2 );
		Output.Write( &DataLen, 2 );
		Output.Write( Header,  19 );
	}

	/* Now write the data block. We'll need to checksum the data as we read it */
	DWORD Offset = pFile->Attributes[ 0 ];

	WORD Pause = 2000;
	WORD DataLen = 2 + pFile->Length;
	
	Output.Write( &BlkID,   1 );
	Output.Write( &Pause,   2 );
	Output.Write( &DataLen, 2 );

	BYTE Flag = 0xFF;

	Output.Write( &Flag, 1 );

	DWORD BytesToGo = pFile->Length;

	BYTE  Buffer[ 16384 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 16384, BytesToGo );

		pSource->ReadRaw( Offset, BytesRead, Buffer );

		for ( WORD i=0; i<BytesRead; i++ ) { Flag ^= Buffer[ i ]; }

		Output.Write( Buffer, BytesRead );

		BytesToGo -= BytesRead;
		Offset    += BytesRead;
	}

	/* Checksum Byte */
	Output.Write( &Flag, 1 );
}

AttrDescriptors SinclairTZXFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Offset in TZX file for data. Hex, visible, disabled */
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

	opt.Dangerous       = true;
	opt.EquivalentValue = 0xFFFFFFFE;
	opt.Name            = L"TZX Block";

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

	/* Vars address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Vars Offset";
	Attrs.push_back( Attr );

	/* Variable in array */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrDanger | AttrFile;;
	Attr.Name  = L"Variable ID";
	Attrs.push_back( Attr );

	/* Offset in TZX file of block. Hex, visible, disabled */
	Attr.Index = 15;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"TZX Block Offset";
	Attrs.push_back( Attr );

	/* Block ID */
	Attr.Index = 14;
	Attr.Type  = AttrVisible | AttrSelect | AttrWarning | AttrFile;
	Attr.Name  = L"TZX Block Type";

	Attr.Options.clear();

	for ( BYTE i=0; i<255; i++ )
	{
		if ( TZXIDs[ i ] == 0x00 ) { break; }

		opt.EquivalentValue = TZXIDs[ i ];
		opt.Name            = TZXBlockTypes[ i ];

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	return Attrs;
}

int SinclairTZXFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	if ( pDirectory->Files[ FileID ].FSFileType == FT_TZX )
	{
		return NUTSError( 0x94,
			L"The selected item is a raw TZX block whose parameters cannot be changed through the properties page.\n\n"
			L"Please use the 'Edit Block' option under 'Sinclair TZX' to change these values."
		);

		return 0;
	}

	for ( BYTE i=1; i<14; i++ )
	{
		pDirectory->Files[ FileID ].Attributes[ i ] = Changes->Attributes[ i ];
	}

	RegenerateTZXSource( TZX_REGEN, 0, 0, nullptr );

	pDirectory->ReadDirectory();

	return 0;
}

void SinclairTZXFileSystem::DescribeResolvedFile( BYTE *Buffer, NativeFile *pFile )
{
	switch ( pFile->Attributes[ 2 ] )
	{
	case 0:
		{
			if ( pFile->Attributes[ 3 ] < 32768 )
			{
				rsprintf(
					Buffer, "Program: LINE %d, vars at offset %04X",
					pFile->Attributes[ 3 ],
					pFile->Attributes[ 4 ]
				);
			}
			else
			{
				rsprintf(
					Buffer, "Program: Vars at offset %04X",
					pFile->Attributes[ 4 ]
				);
			}
		}
		break;

	case 1:
		{
			rsprintf(
				Buffer, "Number Array: %04X bytes of %c$()",
				(DWORD) pFile->Length,
				( pFile->Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 2:
		{
			rsprintf(
				Buffer, "Character Array: %04X bytes of %c$()",
				(DWORD) pFile->Length,
				( pFile->Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 3:
		{
			rsprintf(
				Buffer, "Bytes: %04X bytes at %04X",
				(DWORD) pFile->Length,
				pFile->Attributes[ 1 ]
			);
		}
		break;

	default:
		{
			rsprintf(
				Buffer, "Orphaned block (No header) - %04X bytes",
				(DWORD) pFile->Length
			);
		}
		break;
	}
}

void SinclairTZXFileSystem::RenameResolvedFile( NativeFile *pFile, BYTE *NewName, BYTE *NewExt  )
{
	pFile->Filename = BYTEString( NewName, 10 );

	RegenerateTZXSource( TZX_REGEN, 0, 0, nullptr );

	pDirectory->ReadDirectory();
}

int SinclairTZXFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( pFile->FSFileType == FT_TZX ) 
	{
		return TZXFileSystem::WriteFile( pFile, store );
	}

	if ( ( pFile->FSFileType != FT_SINCLAIR ) && ( pFile->FSFileType != FT_SINCLAIR_DOS ) )
	{
		/* Let the underlying TZX thing do the hard work here */
		return TZXFileSystem::WriteFile( pFile, store );
	}

	CTempFile ExBlocks;

	BYTE BlockID;
	BYTE Header[ 19 ];
	ZeroMemory( Header, 19 );

	Header[ 1 ] = 3;

	rstrncpy( &Header[ 2 ], pFile->Filename, 10 );

	for ( BYTE i=9; i>0; i-- ) { if ( Header[ 2 + i ] == 0 ) { Header[ 2 + i ] = 0x20; } else { break; } }

	WORD DataLen = (WORD) pFile->Length;
	WORD Pause   = 1000;

	BlockID = 0x10;

	* (WORD *) &Header[ 12 ] = DataLen;

	BYTE PType;
	WORD LoadAddr;
	WORD AutoStart;
	WORD VarAddr;
	BYTE Variable;
	BYTE Param3;

	if ( pFile->FSFileType == FT_SINCLAIR ) /* TAP */
	{
		PType = (BYTE) pFile->Attributes[ 2 ];

		LoadAddr  = (WORD) pFile->Attributes[ 1 ];
		VarAddr   = (WORD) pFile->Attributes[ 4 ];
		AutoStart = (WORD) pFile->Attributes[ 3 ];
		Variable  = (BYTE) pFile->Attributes[ 5 ];
	}

	if ( pFile->FSFileType == FT_SINCLAIR_DOS ) /* +3DOS */
	{
		PType = (BYTE) pFile->Attributes[ 6 ];

		LoadAddr  = (WORD) pFile->Attributes[ 5 ];
		VarAddr   = (WORD) pFile->Attributes[ 8 ];
		AutoStart = (WORD) pFile->Attributes[ 7 ];
		Variable  = (BYTE) pFile->Attributes[ 9 ];
	}

	Header[ 1 ] = PType;

	switch ( PType )
	{
	case 0:
		{
			* (WORD *) &Header[ 14 ]   = AutoStart;
			* (WORD *) &Header[ 0x0F ] = VarAddr;
		}
		break;

	case 1:
	case 2:
		{
			Header[ 15 ] = Variable;
		}
		break;

	case 3:
		{
			* (WORD *) &Header[ 14 ] = LoadAddr;
			* (WORD *) &Header[ 16 ] = 32768;
		}
		break;
	}

	for ( BYTE i=0; i<18; i++ ) { Header[ 18 ] ^= Header[ i ]; }

	DataLen += 2;

	WORD HeadLen = 19;
	BYTE Flag    = 0xFF;

	ExBlocks.Seek( 0 );
	store.Seek( 0 );

	if ( PType != 0xFF )
	{
		ExBlocks.Write( &BlockID, 1 );
		ExBlocks.Write( &Pause,   2 );
		ExBlocks.Write( &HeadLen, 2 );
		ExBlocks.Write( Header, 19 );
	}

	Pause = 2000;

	ExBlocks.Write( &BlockID, 1 );
	ExBlocks.Write( &Pause,   2 );
	ExBlocks.Write( &DataLen, 2 );
	ExBlocks.Write( &Flag,    1 );

	/* Copy the data */
	DWORD BytesToGo = pFile->Length;
	BYTE  Buffer[ 16384 ];

	store.Seek( 0 );

	BYTE ck = 0xFF;

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 16384, BytesToGo );

		store.Read( Buffer, BytesRead );

		for ( WORD i=0; i<BytesRead; i++ ) { ck ^= Buffer[ i ]; }

		ExBlocks.Write( Buffer, BytesRead );

		BytesToGo -= BytesRead;
	}

	ExBlocks.Write( &ck, 1 );

	RegenerateTZXSource( TZX_INSERT, pDirectory->Files.size(), 0, &ExBlocks );

	return pDirectory->ReadDirectory();
}

LocalCommands SinclairTZXFileSystem::GetLocalCommands( void )
{
	LocalCommand s  = { LC_IsSeparator, L"" };
	LocalCommand c1 = { LC_OneOrMore,   L"Create Turbo Loader" };

	LocalCommands cmds = TZXFileSystem::GetLocalCommands();

	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c1 );

	return cmds;
}

int SinclairTZXFileSystem::ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
{
	if ( CmdIndex != 11 )
	{
		return TZXFileSystem::ExecLocalCommand( CmdIndex, Selection, hParentWnd );
	}

	if ( Selection.size() > 8 )
	{
		MessageBox( hParentWnd, L"The turbo loader is limited to loading a maximum of 8 files.", L"NUTS Sinclair TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

		return 0;
	}

	/* We're on. The turbo loader binary is already in resources. We'll modify it with the particulars,
	   create a couple of 0x10 blocks with checksums to cover it, then convert the requested blocks 
	   to turbo blocks.

	   Finally insert the loader before the first block, and it should work.

	   We accept standard, turbo and pure data blocks. The latter two will need load addresses supplied
	   by the user. If any *ONE* block overlaps FF00, we'll split that block into two, and use the
	   reserved ROM loader caller to load the second half.
	*/

	HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDR_TURBOLOADER ), RT_RCDATA);
	HGLOBAL hMemory  = LoadResource(hInstance, hResource);
	DWORD dwSize     = SizeofResource(hInstance, hResource);
	LPVOID lpAddress = LockResource(hMemory);

	BYTE *pLoader = (BYTE *) lpAddress;

	std::wstring Program; int t = StringBox( hParentWnd, L"Enter a filename for the turbo loader BASIC program:", Program );

	if( t != 0 ) { return 0; }

	std::wstring Banner;  int b = StringBox( hParentWnd, L"Enter a prompt to be shown at the bottom of the screen:", Banner );

	if ( b != 0 ) { return 0; }

	std::wstring Exec;    int e = StringBox( hParentWnd, L"Enter execution address (0 for return):", Exec );

	WORD ExecAdx = _wtoi( Exec.c_str() );

	if ( b != 0 ) { return 0; }

	/* Block definitions begin at offset 0x20 (0xFE17 in memory space) */
	bool NeedStdBlock = false;
	WORD StdLoad      = 0;
	WORD StdLen       = 0;
	WORD CutOff       = 0;

	WORD      LoadAdx[ 8 ];
	WORD      DataLen[ 8 ];
	DWORD     FileID[ 8 ];
	DWORD     STDID;
	CTempFile StdSource;
	CTempFile LoaderSource;

	DWORD ProgPoint = 0x17 + 9;

	NativeFileIterator iFile;

	BYTE i = 0;

	for ( iFile=Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		if ( iFile->FSFileType == FT_SINCLAIR )
		{
			/* Actual sinclair file - type in 2, load in 1 */
			LoadAdx[ i ] = iFile->Attributes[ 1 ];

			if ( iFile->Attributes[ 2 ] != 0x03 )
			{
				/* EEP - Not actually a bytes file. Ask the user. */
				std::wstring la;

				int r = StringBox( hParentWnd, L"Enter load address (decimal) for non-bytes file:", la );

				if ( r != 0 )
				{
					return 0;
				}

				LoadAdx[ i ] = _wtoi( la.c_str() );
			}

			DataLen[ i ] = iFile->Length;
			FileID[ i ]  = iFile->fileID;
		}
		else
		{
			/* TZX Blob */
			BYTE BlockID = iFile->Attributes[ 14 ];

			if ( ( BlockID != 0x10 ) && ( BlockID != 0x11 ) && ( BlockID != 0x14 ) )
			{
				MessageBox( hParentWnd, L"One of the selected blocks is not of a type that can be turbo loaded.", L"NUTS Sinclair TZX Configuration", MB_ICONERROR | MB_OK );

				return 0;
			}
			else
			{
				if ( iFile->Attributes[ 2 ] != 0x03 )
				{
					/* EEP - Not actually a bytes file. Ask the user. */
					std::wstring la;

					int r = StringBox( hParentWnd, L"Enter load address (decimal) for turbo file:", la );

					if ( r != 0 )
					{
						return 0;
					}

					LoadAdx[ i ] = _wtoi( la.c_str() );
				}

				DataLen[ i ] = iFile->Length;
				FileID[ i ]  = iFile->fileID;
			}
		}

		if ( ( LoadAdx[ i ] + DataLen[ i ] ) > 0xFE00 )
		{
			/* Danger Will Robinson! */
			NeedStdBlock = true;
			CutOff       = 0xFE00 - LoadAdx[ i ];
			StdLen       = DataLen[ i ] - CutOff;
			DataLen[ i ] = DataLen[ i ] - StdLen;
			STDID        = iFile->fileID;
		}
	}

	/* All details now confirmed - we're committed now */
	for ( i=0; i<Selection.size(); i++ )
	{
		* (WORD *) &pLoader[ ProgPoint + 1 ] = DataLen[ i ];
		* (WORD *) &pLoader[ ProgPoint + 5 ] = LoadAdx[ i ];

		/* Call block converter */

		ProgPoint += 0x0A; /* 10 bytes per call block */
	}

	/* Insert a JP instruction if there are less than 8 blocks. */
	if ( Selection.size() < 8 )
	{
		pLoader[ ProgPoint++ ] = 0xC3;
		pLoader[ ProgPoint++ ] = 0x67;
		pLoader[ ProgPoint++ ] = 0xFE;
	}
	
	/* If a standard block wasn't needed, stick a ret in here now */
	ProgPoint = 0x69 + 9;

	* (WORD *) &pLoader[ ProgPoint ] = ExecAdx;

	if ( !NeedStdBlock )
	{
		if ( ExecAdx == 0 )
		{
			ProgPoint = 0x67 + 9;

			pLoader[ ProgPoint ] = 0xC9;
		}
		else
		{
			ProgPoint = 0x6C + 9;

			pLoader[ ProgPoint ] = 0xC9;
		}
	}
	else
	{
		/* Fill in the details */
		ProgPoint = 0x6D + 9;

		* (WORD *) &pLoader[ ProgPoint ] = StdLen;

		ProgPoint = 0x71 + 9;

		* (WORD *) &pLoader[ ProgPoint ] = 0xFE00;

		if ( ExecAdx == 0 )
		{
			/* Blank out the stack fernangling */
			for ( BYTE f=0; f<5; f++ ) { pLoader[ 0x67 + 9 + f ] = 0x00; }
		}
	}

	/* Copy some strings - offset 0x254 - fill in with blanks - there are 30*/
	memset( &pLoader[ 0x254 ], 0x20, 31 );

	int l = Banner.length();

	if ( l > 30 ) { l = 30; }

	int s = 16 - ( l / 2 );

	memcpy( &pLoader[ 0x254 + s ], AString( (WCHAR *) Banner.c_str() ), l );

	/* Create a bootloader */
	WORD Pause = 1000;
	WORD DLen  = 19;
	WORD LLen  = dwSize;
	WORD HLen  = LLen + 2;
	BYTE Flag  = 0xFF;
	BYTE Blk   = 0x10;
	BYTE Header[ 19 ];

	/* Create header */
	Header[ 0 ] = 0x00;
	Header[ 1 ] = 0x00; /* Program */

	memset( &Header[ 2 ], 0x20, 10 );
	memcpy( &Header[ 2 ], AString( (WCHAR *) Program.c_str() ), min( 10, Program.length() ) );

	* (WORD *) &Header[ 12 ] = LLen;
	* (WORD *) &Header[ 14 ] = 30; /* Autostart line */

	Header[ 18 ] = 0;

	for ( BYTE n=0; n<18; n++ ) { Header[ 18 ] ^= Header[ n ]; }

	LoaderSource.Seek( 0 );
	LoaderSource.Write( &Blk,    1 );
	LoaderSource.Write( &Pause,  2 );
	LoaderSource.Write( &DLen,   2 );
	LoaderSource.Write( Header, 19 );

	if ( !NeedStdBlock )
	{
		Pause = 2000;
	}

	LoaderSource.Write( &Blk,    1 );
	LoaderSource.Write( &Pause,  2 );
	LoaderSource.Write( &HLen,   2 );
	LoaderSource.Write( &Flag,   1 );
	LoaderSource.Write( pLoader, dwSize );

	for ( WORD i=0; i<LLen; i++ ) { Flag ^= pLoader[ i ]; }

	LoaderSource.Write( &Flag,   1 );

	/*  Do the insertion now */
	RegenerateTZXSource( TZX_INSERT, Selection[ 0 ].fileID, 0, &LoaderSource );

	/* If we needed a std block, create it here */
	pDirectory->ReadDirectory();

	STDID++;

	if ( NeedStdBlock )
	{
		Pause = 2000;
		Flag  = 0xFF;
		LLen  = StdLen;
		HLen  = StdLen + 2;

		BYTE *pExtra = new BYTE[ StdLen ];

		CTempFile ExtraSource;

		if ( pDirectory->Files[ STDID ].FSFileType != FT_TZX )
		{
			ReadFile( STDID, ExtraSource );

			ExtraSource.Seek( CutOff );
			ExtraSource.Read( pExtra, StdLen );
		}
		else
		{
			ReadBlock( STDID, ExtraSource );

			DWORD Lead = 0;

			if ( pDirectory->Files[ STDID ].Attributes[ 14 ] == 0x10 ) { Lead = 0x05 ; }
			if ( pDirectory->Files[ STDID ].Attributes[ 14 ] == 0x11 ) { Lead = 0x13 ; }
			if ( pDirectory->Files[ STDID ].Attributes[ 14 ] == 0x14 ) { Lead = 0x0B ; }

			ExtraSource.Seek( CutOff + Lead );
			ExtraSource.Read( pExtra, StdLen );
		}

		StdSource.Seek( 0 );
		StdSource.Write( &Blk,    1 );
		StdSource.Write( &Pause,  2 );
		StdSource.Write( &HLen,   2 );
		StdSource.Write( &Flag,   1 );
		StdSource.Write( pExtra, StdLen );

		for ( WORD i=0; i<LLen; i++ ) { Flag ^= pExtra[ i ]; }

		StdSource.Write( &Flag,   1 );

		delete pExtra;

		RegenerateTZXSource( TZX_INSERT, Selection.back().fileID + 2, 0, &StdSource );
	}

	return CMDOP_REFRESH;
}

void SinclairTZXFileSystem::MakeResolvedAudio( NativeFile *pFile, CTempFile &output, TapeIndex &indexes )
{
	BYTE TZXHeader[ 32 ];
	BYTE Flag;

	TapeCue Cue;

	if ( pFile->Attributes[ 2 ] != 0xFFFFFFFF )
	{
		pSource->ReadRaw( pFile->Attributes[ 15 ], 32, TZXHeader );

		Flag = TZXHeader[ 0x05 ];

		Cue.IndexPtr = AudioPtr;

		MakeAudioFromBytes( Flag, Cue, pFile->Attributes[ 15 ] + 5, 19, output, TZXHeader );

		indexes.Cues.push_back( Cue );

		pSource->ReadRaw( pFile->Attributes[ 15 ] + 19 + 5, 32, TZXHeader );

		Flag = TZXHeader[ 0x05 ];

		Cue.IndexPtr = AudioPtr;

		MakeAudioFromBytes( Flag, Cue, pFile->Attributes[ 15 ] + 19 + 5 + 5, pFile->Length, output, TZXHeader );

		indexes.Cues.push_back( Cue );
	}
	else
	{
		/* ORPHAN BLK */
		pSource->ReadRaw( pFile->Attributes[ 15 ], 32, TZXHeader );

		Flag = TZXHeader[ 0x05 ];

		Cue.IndexPtr = AudioPtr;

		MakeAudioFromBytes( Flag, Cue, pFile->Attributes[ 15 ] + 5, pFile->Length, output, TZXHeader );

		indexes.Cues.push_back( Cue );
	}
}

void SinclairTZXFileSystem::MakeAudioFromBytes( BYTE Flag, TapeCue &Cue, QWORD Offset, DWORD Length, CTempFile &output, BYTE *Header )
{
	if ( Flag == 0x00 )
	{
		Cue.IndexName = GetZXFilename( &Header[ 0x06 ] );
		Cue.Flags     = 0;

		MakeAudioPulses( 2168, 8063, &output );
	}
	else
	{
		Cue.IndexName = L"Data Block, " + std::to_wstring( (QWORD) ( Length ) ) + L" bytes";
		Cue.Flags     = 0;

		MakeAudioPulses( 2168, 3223, &output );
	}

	MakeAudioPulse( 667, 753, &output );

	DWORD BytesToGo = Length;

	BYTE  Buffer[ 16384 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 16384, BytesToGo );

		pSource->ReadRaw( Offset, BytesRead, Buffer );

		MakeAudioBytes( Buffer, BytesRead, &output );

		Offset    += BytesRead;
		BytesToGo -= BytesRead;
	}

	MakePause( * (WORD *) &Header[ 0x01 ], &output );
}