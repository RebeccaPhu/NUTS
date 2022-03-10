#include "StdAfx.h"
#include "ISO9660Directory.h"
#include "NUTSError.h"
#include "libfuncs.h"
#include "ExtensionRegistry.h"
#include "ISOFuncs.h"

#include <time.h>
#include <algorithm>

ISO9660Directory::ISO9660Directory( DataSource *pDataSource ) : Directory( pDataSource )
{
	pPriVolDesc = nullptr;
	pJolietDesc = nullptr;

	UsingJoliet = false;

	PathStackExtent.clear();
	PathStackSize.clear();
}


ISO9660Directory::~ISO9660Directory(void)
{
	ClearAuxData();
}

void ISO9660Directory::ClearAuxData()
{
	for ( NativeFileIterator iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		if ( iFile->pAuxData != nullptr )
		{
			delete iFile->pAuxData;
		}

		iFile->pAuxData = nullptr;
		iFile->lAuxData = 0;
	}
}

int	ISO9660Directory::ReadDirectory(void)
{
	if ( UsingJoliet )
	{
		return ReadJDirectory();
	}

	ClearAuxData();

	Files.clear();

	FileFOPData.clear();

	DWORD FileID = 0;

	DWORD DirSect  = DirSector;
	DWORD BOffset  = 0;
	DWORD DirCount = 0;

	AutoBuffer Sector( pPriVolDesc->SectorSize );

	bool NeedRead = true;

	while ( DirCount < DirLength )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) Sector, pPriVolDesc->SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		NeedRead = false;

		BYTE EntrySize = Sector[ BOffset + 0 ];

		if ( EntrySize == 0 )
		{
			// Sector not big enough to contain the entry. Move to next sector.
			DirSect++;

			NeedRead = true;

			DirCount += ( pPriVolDesc->SectorSize - BOffset );

			BOffset = 0;

			continue;
		}

		NativeFile file;

		// . or .. - skip
		bool SkipFile = false;

		if ( ( Sector[ BOffset + 32 ] == 1 ) && ( Sector[ BOffset + 33 ] < 2 ) )
		{
			SkipFile = true;
		}

		if ( !SkipFile )
		{
			bool IsAssoc = false;

			file.EncodingID  = ENCODING_ASCII;
			file.Filename    = BYTEString( &Sector[ BOffset + 33 ], Sector[ BOffset + 32 ] );
			file.FSFileType  = FT_WINDOWS;
			file.FSFileTypeX = FT_ISO;
			file.Type        = FT_Arbitrary;
			file.Icon        = FT_Arbitrary;
			file.Length      = * (DWORD *) &Sector[ BOffset + 10 ];
			file.fileID      = FileID;

			file.Attributes[ ISOATTR_START_EXTENT ] = * (DWORD *) (BYTE *) &Sector[ BOffset +  2 ];
			file.Attributes[ ISOATTR_REVISION     ] = 0;
			file.Attributes[ ISOATTR_FORK_EXTENT  ] = 0;
			file.Attributes[ ISOATTR_FORK_LENGTH  ] = 0;
			file.Attributes[ ISOATTR_VOLSEQ       ] = Sector[ BOffset + 28 ];
			file.Attributes[ ISOATTR_TIMESTAMP    ] = ConvertISOTime( &Sector[ BOffset + 18 ] );

			BYTE ISOFlags = Sector[ BOffset + 25 ];

			if ( FSID == FSID_ISOHS )
			{
				ISOFlags = Sector[ BOffset + 24 ];
			}

			// Do some filename mangling
			if ( ( ISOFlags & 2 ) == 0 )
			{
				// Doesn't apply to directories
				for ( WORD i=file.Filename.length() - 1; i != 0; i-- )
				{
					if ( file.Filename[ i ] == ';' )
					{
						// File has a revision ID
						file.Attributes[ ISOATTR_REVISION ] = atoi( (char *) (BYTE *) file.Filename + i + 1 );

						file.Filename = BYTEString( (BYTE *) file.Filename, i );
					}

					if ( file.Filename[ i ] == '.' )
					{
						file.Extension = BYTEString( (BYTE *) file.Filename + i + 1 );
						file.Filename  = BYTEString( (BYTE *) file.Filename, i );

						if ( file.Extension.length() > 0 )
						{
							file.Flags |= FF_Extension;
						}
					}
				}
			}

			// If we have a matching Associated File, we need to copy some details.. BUT ONLY FOR ISO9660!
			if ( FSID == FSID_ISO9660 )
			{
				if ( rstrncmp( file.Filename, AssocFile.Filename, max( file.Filename.length(), AssocFile.Filename.length() ) ) )
				{
					// Note that only the most recent associated file survives
					file.Attributes[ ISOATTR_FORK_EXTENT ] = AssocFile.Attributes[ ISOATTR_START_EXTENT ];
					file.Attributes[ ISOATTR_FORK_LENGTH ] = AssocFile.Length;

					file.ExtraForks = 1;
				}
			}

			if ( ISOFlags & 2 )
			{
				file.Flags |= FF_Directory;
				file.Type   = FT_Directory;
				file.Icon   = FT_Directory;
			}
			else if ( ISOFlags & 4 )
			{
				// "Associated File" - this is basically a fork. Store it if it's ISO9660, as it applies t
				// the next directory entry. If we're High Sierra, make the change now.
				if ( FSID == FSID_ISO9660 )
				{
					AssocFile = file;
				}
				else
				{
					// Sanity check
					if ( Files.size() > 0 )
					{
						NativeFileIterator iPrevFile = Files.begin() + ( Files.size() - 1 );

						iPrevFile->Attributes[ ISOATTR_FORK_EXTENT ] = file.Attributes[ ISOATTR_START_EXTENT ];
						iPrevFile->Attributes[ ISOATTR_FORK_LENGTH ] = file.Length;
					}
				}

				IsAssoc = true;
			}

			if ( !IsAssoc )
			{
				if ( file.Flags & FF_Extension )
				{
					ExtDef ed = ExtReg.GetTypes( std::wstring( UString( (char *) file.Extension ) ) );

					file.Type = ed.Type;
					file.Icon = ed.Icon;
				}

				if ( file.Type == FT_Text )
				{
					file.XlatorID = TUID_TEXT;
				}

				FileID++;

				Files.push_back( file );

				BYTE *SUSP      = (BYTE *) &Sector[ BOffset ];
				BYTE SUSPOffset = 33 + Sector[ BOffset + 32 ];

				if ( SUSPOffset & 1 ) { SUSPOffset++; }

				BYTE SUSPLength = Sector[ BOffset ] - SUSPOffset;

				if ( !CloneWars )
				{
					if ( !RockRidge( &Sector[ BOffset ], &Files.back() ) )
					{
						// FOP this
						FOPData fop;

						fop.DataType  = FOP_DATATYPE_CDISO;
						fop.Direction = FOP_ReadEntry;
						fop.pFile     = (void *) &Files.back();
						fop.pFS       = pSrcFS;
				
						fop.pXAttr = &SUSP[ SUSPOffset ];
						fop.lXAttr = Sector[ BOffset ] - SUSPOffset;

						if ( ProcessFOP( &fop ) )
						{
							if ( fop.ReturnData.Identifier != L"" )
							{
								FileFOPData[ file.fileID ] = fop.ReturnData;
							}
						}

					}
				}

				if ( SUSPLength > 0 )
				{
					Files.back().pAuxData = new BYTE[ SUSPLength ];
					Files.back().lAuxData = SUSPLength;

					memcpy( Files.back().pAuxData, &SUSP[ SUSPOffset ], SUSPLength );
				}
			}
		}

		BOffset += EntrySize;

		DirCount += EntrySize;
	}

	// Now that we have the whole dir, see if plugins want a second suck of the sav, as Dave Jones would say.
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_PostRead;
	fop.pFile     = nullptr;
	fop.pFS       = pSrcFS;				
	fop.pXAttr    = nullptr;
	fop.lXAttr    = 0;

	ProcessFOP( &fop );

	return 0;
}

int	ISO9660Directory::ReadJDirectory(void)
{
	ClearAuxData();

	Files.clear();

	FileFOPData.clear();

	DWORD FileID = 0;

	DWORD DirSect  = JDirSector;
	DWORD BOffset  = 0;
	DWORD DirCount = 0;

	AutoBuffer Sector( pJolietDesc->SectorSize );

	bool NeedRead = true;

	while ( DirCount < JDirLength )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadSectorLBA( DirSect, (BYTE *) Sector, pJolietDesc->SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		NeedRead = false;

		BYTE EntrySize = Sector[ BOffset + 0 ];

		if ( EntrySize == 0 )
		{
			// Sector not big enough to contain the entry. Move to next sector.
			DirSect++;

			NeedRead = true;

			DirCount += ( pJolietDesc->SectorSize - BOffset );

			BOffset = 0;

			continue;
		}

		NativeFile file;

		// . or .. - skip
		bool SkipFile = false;

		if ( ( Sector[ BOffset + 32 ] == 1 ) && ( Sector[ BOffset + 33 ] < 2 ) )
		{
			SkipFile = true;
		}

		if ( !SkipFile )
		{
			AutoBuffer fname( ( Sector[ BOffset + 32 ] + 1 ) * 2 );

			memcpy( (BYTE *) fname, &Sector[ BOffset + 33 ],  Sector[ BOffset + 32 ] * 2 );

			fname[ Sector[ BOffset + 32 ] * 2 ] = 0;

			JolietStrTerm( fname, Sector[ BOffset + 32 ] );

			file.EncodingID = ENCODING_ASCII;
			file.Filename   = BYTEString( fname, Sector[ BOffset + 32 ] );
			file.FSFileType = FT_Windows;
			file.Type       = FT_Arbitrary;
			file.Icon       = FT_Arbitrary;
			file.Length     = * (DWORD *) &Sector[ BOffset + 10 ];
			file.fileID     = FileID;

			file.Attributes[ ISOATTR_START_EXTENT ] = * (DWORD *) (BYTE *) &Sector[ BOffset +  2 ];
			file.Attributes[ ISOATTR_REVISION     ] = 0;
			file.Attributes[ ISOATTR_FORK_EXTENT  ] = 0;
			file.Attributes[ ISOATTR_FORK_LENGTH  ] = 0;
			file.Attributes[ ISOATTR_VOLSEQ       ] = Sector[ BOffset + 28 ];
			file.Attributes[ ISOATTR_TIMESTAMP    ] = ConvertISOTime( &Sector[ BOffset + 18 ] );

			BYTE ISOFlags = Sector[ BOffset + 25 ];

			// NOTE: Joliet doesn't apply to High Sierra

			// Do some filename mangling
			if ( ( ISOFlags & 2 ) == 0 )
			{
				// Doesn't apply to directories
				for ( WORD i=file.Filename.length() - 1; i != 0; i-- )
				{
					if ( file.Filename[ i ] == ';' )
					{
						// File has a revision ID
						file.Attributes[ ISOATTR_REVISION ] = atoi( (char *) (BYTE *) file.Filename + i + 1 );

						file.Filename = BYTEString( (BYTE *) file.Filename, i );
					}

					if ( file.Filename[ i ] == '.' )
					{
						file.Extension = BYTEString( (BYTE *) file.Filename + i + 1 );
						file.Filename  = BYTEString( (BYTE *) file.Filename, i );

						if ( file.Extension.length() > 0 )
						{
							file.Flags |= FF_Extension;
						}
					}
				}
			}

			// NOTE: Joliet is never used to support associated files (forks)
			if ( ISOFlags & 2 )
			{
				file.Flags |= FF_Directory;
				file.Type   = FT_Directory;
				file.Icon   = FT_Directory;
			}

			if ( file.Flags & FF_Extension )
			{
				ExtDef ed = ExtReg.GetTypes( std::wstring( UString( (char *) file.Extension ) ) );

				file.Type = ed.Type;
				file.Icon = ed.Icon;
			}

			if ( file.Type == FT_Text )
			{
				file.XlatorID = TUID_TEXT;
			}

			FileID++;

			Files.push_back( file );

			BYTE *SUSP      = (BYTE *) &Sector[ BOffset ];
			BYTE SUSPOffset = 33 + Sector[ BOffset + 32 ];

			if ( SUSPOffset & 1 ) { SUSPOffset++; }

			BYTE SUSPLength = Sector[ BOffset ] - SUSPOffset;

			if ( !CloneWars )
			{
				// NOTE: Joliet won't have Rock Ridge extensions, but might have some kind of arcane SUSP extension.

				// FOP this
				FOPData fop;

				fop.DataType  = FOP_DATATYPE_CDISO;
				fop.Direction = FOP_ReadEntry;
				fop.pFile     = (void *) &Files.back();
				fop.pFS       = pSrcFS;
				
				fop.pXAttr = &SUSP[ SUSPOffset ];
				fop.lXAttr = Sector[ BOffset ] - SUSPOffset;

				if ( ProcessFOP( &fop ) )
				{
					if ( fop.ReturnData.Identifier != L"" )
					{
						FileFOPData[ file.fileID ] = fop.ReturnData;
					}
				}
			}

			if ( SUSPLength > 0 )
			{
				Files.back().pAuxData = new BYTE[ SUSPLength ];
				Files.back().lAuxData = SUSPLength;

				memcpy( Files.back().pAuxData, &SUSP[ SUSPOffset ], SUSPLength );
			}
		}

		BOffset += EntrySize;

		DirCount += EntrySize;
	}

	// Now that we have the whole dir, see if plugins want a second suck of the sav, as Dave Jones would say.
	FOPData fop;

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_PostRead;
	fop.pFile     = nullptr;
	fop.pFS       = pSrcFS;				
	fop.pXAttr    = nullptr;
	fop.lXAttr    = 0;

	ProcessFOP( &fop );

	return 0;
}

static bool DirectorySort( NativeFile &a, NativeFile &b )
{
	BYTE NameA[ 256 ];
	BYTE NameB[ 256 ];

	MakeISOFilename( &a, NameA );
	MakeISOFilename( &b, NameB );

	// std::sort wants true if a < b
	for ( int i=0; i<min(a.Filename.length(), b.Filename.length() ); i++ )
	{
		if ( NameA[ i ] < NameB[ i ] )
		{
			return true;
		}
		else if ( NameA[ i ] > NameB[ i ] )
		{
			return false;
		}
	}

	// If we got here then the directories have the same name up to the point of the minimum length of the two.
	if ( a.Filename.length() < b.Filename.length() )
	{
		return true;
	}

	return false;
}

int	ISO9660Directory::WriteDirectory(void)
{
	AutoBuffer SectorBuffer( pPriVolDesc->SectorSize );

	BYTE *sector = (BYTE *) SectorBuffer;

	NativeFileIterator iFile;

	DWORD SBytes = 0;

	BYTE ISOFilename[ 256 ];
	
	DWORD DirSect = DirSector;

	ZeroMemory( sector, pPriVolDesc->SectorSize );

	// Sort the directory - ISO standard requires ISO filenames are correctly sorted.
	std::sort( Files.begin(), Files.end(), DirectorySort );

	// We must fill in . and .., because reasons.
	BYTE *pEnt = sector;

	// . = this
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], DirSector ); WBEDWORD( &pEnt[  6 ], DirSector );
	WLEDWORD( &pEnt[ 10 ], DirLength ); WBEDWORD( &pEnt[ 14 ], DirLength );
	ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 0;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	pEnt += 0x22;

	// .. = parent
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], ParentSector ); WBEDWORD( &pEnt[  6 ], ParentSector );
	WLEDWORD( &pEnt[ 10 ], ParentLength ); WBEDWORD( &pEnt[ 14 ], ParentLength );
	ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 1;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	SBytes = 0x44;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		MakeISOFilename( &*iFile, ISOFilename );

		DWORD ForkNum = 0;

		// In ISO9660, the fork comes before the file. In High Sierra, it comes after. (thanks)
		if ( FSID == FSID_ISOHS )
		{
			ForkNum = iFile->ExtraForks;
		}

		for ( int f=0; f < ( 1 + iFile->ExtraForks ); f++ )
		{
			// Calculate the size of the directory entry, as we may need to move along.
			DWORD FBytes = rstrnlen( ISOFilename, 220 );
			DWORD EBytes = 33 + FBytes;

			if ( ( EBytes & 1 ) == 1 ) { EBytes++; }

			EBytes += iFile->lAuxData;

			if ( ( EBytes & 1 ) == 1 ) { EBytes++; }

			if ( ( SBytes + EBytes ) >= pPriVolDesc-> SectorSize )
			{
				// Write this sector out, and get a new one.
				SetISOHints( pSource, false, false );

				if ( pSource->WriteSectorLBA( DirSect, sector, pPriVolDesc->SectorSize ) != DS_SUCCESS )
				{
					return -1;
				}

				DirSect++;

				ZeroMemory( sector, pPriVolDesc->SectorSize );

				SBytes = 0;
			}

			DWORD Extent = iFile->Attributes[ ISOATTR_START_EXTENT ];
			DWORD DataSz = iFile->Length;
			BYTE  Flags  = 0;

			if ( iFile->Flags & FF_Directory )
			{
				Flags |= 0x2;
			}

			if ( ( f == ForkNum ) && ( iFile->ExtraForks > 0 ) )
			{
				Extent = iFile->Attributes[ ISOATTR_FORK_EXTENT ];
				DataSz = iFile->Attributes[ ISOATTR_FORK_LENGTH ];
				Flags |= 0x4;
			}

			// Now we can write the entry proper, both-endian dwords and all.
			BYTE *pEnt = &sector[ SBytes ];

			pEnt[ 0 ] = (BYTE) EBytes;
			pEnt[ 1 ] = 0; // Don't support extended attributes
		
			* (DWORD *) &pEnt[  2 ] = Extent; WBEDWORD( &pEnt[  6 ], Extent );
			* (DWORD *) &pEnt[ 10 ] = DataSz; WBEDWORD( &pEnt[ 14 ], DataSz );

			ConvertUnixTime( &pEnt[ 18 ], iFile->Attributes[ ISOATTR_TIMESTAMP ] );

			pEnt[ 25 ] = Flags;

			if ( FSID == FSID_ISOHS )
			{
				pEnt[ 24 ] = Flags; pEnt[ 25 ] = 0;
			}

			pEnt[ 26 ] = 0; // File Unit Size
			pEnt[ 27 ] = 0; // Interleave
			* (WORD *) &pEnt[ 28 ] = iFile->Attributes[ ISOATTR_VOLSEQ ];
			WBEWORD( &pEnt[ 30 ], iFile->Attributes[ ISOATTR_VOLSEQ ] );
			pEnt[ 32 ] = (BYTE) FBytes;

			rstrncpy( &pEnt[ 33 ], ISOFilename, 220 );

			if ( iFile->lAuxData != 0 )
			{
				pEnt += 33 + FBytes;

				if ( ( FBytes  & 1 ) == 0 )
				{
					*pEnt = 0;

					pEnt++;
				}

				memcpy( pEnt, iFile->pAuxData, iFile->lAuxData );
			}

			SBytes += EBytes;
		}
	}

	if ( SBytes > 0 )
	{
		SetISOHints( pSource, true, true );

		if ( pSource->WriteSectorLBA( DirSect, sector, pPriVolDesc->SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	return 0;
}

void ISO9660Directory::WriteJDirectory( DWORD DirSector, DWORD JParentSector, DWORD JParentSize )
{
	if ( pJolietDesc == nullptr ) { return; }

	AutoBuffer SectorBuffer( pJolietDesc->SectorSize );

	BYTE *sector = (BYTE *) SectorBuffer;

	NativeFileIterator iFile;

	DWORD SBytes = 0;

	DWORD DirSect = DirSector;

	ZeroMemory( sector, pJolietDesc->SectorSize );

	// Sort the directory - ISO standard requires ISO filenames are correctly sorted.
	std::sort( Files.begin(), Files.end(), DirectorySort );

	// We must fill in . and .., because reasons.
	BYTE *pEnt = sector;

	// . = this
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], DirSector ); WBEDWORD( &pEnt[  6 ], DirSector );
	WLEDWORD( &pEnt[ 10 ], DirLength ); WBEDWORD( &pEnt[ 14 ], ProjectedDirectorySize() * pJolietDesc->SectorSize );
	ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 0;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	pEnt += 0x22;

	// .. = parent
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], ParentSector ); WBEDWORD( &pEnt[  6 ], JParentSector );
	WLEDWORD( &pEnt[ 10 ], ParentLength ); WBEDWORD( &pEnt[ 14 ], JParentSize );
	ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 1;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	SBytes = 0x44;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		DWORD ForkNum = 0;

		// In ISO9660, the fork comes before the file. In High Sierra, it comes after. (thanks)
		if ( FSID == FSID_ISOHS )
		{
			ForkNum = iFile->ExtraForks;
		}

		for ( int f=0; f < ( 1 + iFile->ExtraForks ); f++ )
		{
			// Calculate the size of the directory entry, as we may need to move along.
			DWORD FBytes = min( iFile->Filename.length(), 64 ) << 1;
			DWORD EBytes = 33 + FBytes;

			if ( ( EBytes & 1 ) == 1 ) { EBytes++; }

			EBytes += iFile->lAuxData;

			if ( ( EBytes & 1 ) == 1 ) { EBytes++; }

			if ( ( SBytes + EBytes ) >= pJolietDesc-> SectorSize )
			{
				// Write this sector out, and get a new one.
				SetISOHints( pSource, false, false );

				if ( pSource->WriteSectorLBA( DirSect, sector, pJolietDesc->SectorSize ) != DS_SUCCESS )
				{
					return;
				}

				DirSect++;

				ZeroMemory( sector, pPriVolDesc->SectorSize );

				SBytes = 0;
			}

			DWORD Extent = iFile->Attributes[ ISOATTR_START_EXTENT ];
			DWORD DataSz = iFile->Length;
			BYTE  Flags  = 0;

			if ( iFile->Flags & FF_Directory )
			{
				Flags |= 0x2;
			}

			if ( ( f == ForkNum ) && ( iFile->ExtraForks > 0 ) )
			{
				Extent = iFile->Attributes[ ISOATTR_FORK_EXTENT ];
				DataSz = iFile->Attributes[ ISOATTR_FORK_LENGTH ];
				Flags |= 0x4;
			}

			// Now we can write the entry proper, both-endian dwords and all.
			BYTE *pEnt = &sector[ SBytes ];

			pEnt[ 0 ] = (BYTE) EBytes;
			pEnt[ 1 ] = 0; // Don't support extended attributes
		
			* (DWORD *) &pEnt[  2 ] = Extent; WBEDWORD( &pEnt[  6 ], Extent );
			* (DWORD *) &pEnt[ 10 ] = DataSz; WBEDWORD( &pEnt[ 14 ], DataSz );

			ConvertUnixTime( &pEnt[ 18 ], iFile->Attributes[ ISOATTR_TIMESTAMP ] );

			pEnt[ 25 ] = Flags;

			if ( FSID == FSID_ISOHS )
			{
				pEnt[ 24 ] = Flags; pEnt[ 25 ] = 0;
			}

			pEnt[ 26 ] = 0; // File Unit Size
			pEnt[ 27 ] = 0; // Interleave
			* (WORD *) &pEnt[ 28 ] = iFile->Attributes[ ISOATTR_VOLSEQ ];
			WBEWORD( &pEnt[ 30 ], iFile->Attributes[ ISOATTR_VOLSEQ ] );
			pEnt[ 32 ] = (BYTE) FBytes;

			WCHAR BEStr[ 256 ];

			ISOJolietStore( (BYTE *) BEStr, iFile->Filename, FBytes );

			memcpy( &pEnt[ 33 ], BEStr, FBytes );

			if ( iFile->lAuxData != 0 )
			{
				pEnt += 33 + FBytes;

				if ( ( FBytes  & 1 ) == 0 )
				{
					*pEnt = 0;

					pEnt++;
				}

				memcpy( pEnt, iFile->pAuxData, iFile->lAuxData );
			}

			SBytes += EBytes;
		}
	}

	if ( SBytes > 0 )
	{
		SetISOHints( pSource, true, true );

		if ( pSource->WriteSectorLBA( DirSect, sector, pJolietDesc->SectorSize ) != DS_SUCCESS )
		{
			return;
		}
	}

	return;
}

bool ISO9660Directory::RockRidge( BYTE *pEntry, NativeFile *pTarget )
{
	BYTE pOff  = 33 + pEntry[ 32 ];

	if ( pOff & 1 ) { pOff++; }

	while (( pOff < pEntry[ 0 ] ) && ( pOff >= 32 ) )
	{
		bool HaveTag = false;

		BYTE TagLen = pEntry[ pOff + 2 ];

		if ( memcmp( &pEntry[ pOff ], (void *) "NM", 2 ) == 0 )
		{
			HaveTag = true;

			BYTE NMFlags = pEntry[ pOff + 4 ];

			if ( ( NMFlags & 6 ) == 0 )
			{
				BYTE NMLen = TagLen - 5;

				BYTEString NewName = BYTEString( &pEntry[ pOff + 5 ], NMLen );

				pTarget->Filename = NewName;

				for ( BYTE c=0; c<NMLen; c++ )
				{
					if ( NewName[ c ] == '.' )
					{
						if ( c != 0 )
						{
							pTarget->Filename = BYTEString( &pEntry[ pOff + 5 ], c );

							if ( c < NMLen - 2 )
							{
								pTarget->Extension = BYTEString( &pEntry[ pOff + 5 + c + 1 ], NMLen - ( c + 1 ) );

								pTarget->Flags |= FF_Extension;
							}
						}

						break;
					}
				}
			}
		}

		if ( memcmp( &pEntry[ pOff ], (void *) "PX", 2 ) == 0 ) { HaveTag = true; }
		if ( memcmp( &pEntry[ pOff ], (void *) "TF", 2 ) == 0 ) { HaveTag = true; }

		pOff += TagLen;

		if ( !HaveTag ) { break; }
	}

	return false;
}

DWORD ISO9660Directory::ConvertISOTime( BYTE *pTimestamp )
{
	struct tm ISOTime;

	ISOTime.tm_hour  = pTimestamp[ 3 ];
	ISOTime.tm_isdst = 0;
	ISOTime.tm_min   = pTimestamp[ 4 ];
	ISOTime.tm_sec   = pTimestamp[ 5 ];
	ISOTime.tm_year  = pTimestamp[ 0 ];
	ISOTime.tm_mday  = pTimestamp[ 2 ];
	ISOTime.tm_mon   = pTimestamp[ 1 ] - 1;

	DWORD UnixTime = (DWORD) mktime( &ISOTime );

	return UnixTime;
}

void ISO9660Directory::ConvertUnixTime( BYTE *pTimestamp, DWORD Unixtime )
{
	time_t thistime = (time_t) Unixtime;

	struct tm *pISOTime = localtime( &thistime );

	pTimestamp[ 3 ] = pISOTime->tm_hour;
	pTimestamp[ 4 ] = pISOTime->tm_min;
	pTimestamp[ 5 ] = pISOTime->tm_sec;
	pTimestamp[ 0 ] = pISOTime->tm_year;
	pTimestamp[ 2 ] = pISOTime->tm_mday;
	pTimestamp[ 1 ] = pISOTime->tm_mon + 1;
}

// Calculates how many bytes are required to store the directory contiguously (excepting
// sector boundaries), taking into account the System Use area on each entry, and returns
// the size as the number of sectors required.
DWORD ISO9660Directory::ProjectedDirectorySize( bool Joliet )
{
	DWORD DBytes = 0;
	DWORD SBytes = 0;

	// Add the fixed sizes for . and ..
	DBytes += 0x22 + 0x22;
	SBytes += 0x22 + 0x22;

	BYTE ISOFilename[ 256 ];

	for ( NativeFileIterator iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		// We must do this once for the file, and once more for each fork, as in ISO
		// a fork creates another "file" that has the "associated file" bit set.

		for ( DWORD f=0; f < ( 1 + iFile->ExtraForks ); f++ )
		{
			// The size of each file is 33 bytes + the filename length.
			// Then add one if the filename length is even, so that the entry is even.
			// Except then add on the SUSP size. Then add another byte if the result is odd.
			DWORD FBytes = 33;

			MakeISOFilename( &*iFile, ISOFilename );
		
			FBytes += rstrnlen( ISOFilename, 240 );

			if ( Joliet ) { FBytes <<= 1; }
		
			if ( ( FBytes & 1 ) == 1 ) { FBytes++; }

			FBytes += iFile->lAuxData;

			if ( FBytes & 1 ) { FBytes++; }

			if ( ( SBytes + FBytes ) >= pPriVolDesc->SectorSize )
			{
				// OOPS. This doesn't fit, skip on.
				DBytes += ( pPriVolDesc->SectorSize - SBytes );

				SBytes = 0;
			}

			SBytes += FBytes;
			DBytes += FBytes;
		}
	}

	DWORD NeededSectors = ( DBytes + ( pPriVolDesc->SectorSize - 1 ) ) / pPriVolDesc->SectorSize;

	return NeededSectors;
}


void ISO9660Directory::Push( DWORD Extent, DWORD Length )
{
	if ( !UsingJoliet )
	{
		PathStackExtent.push_back( DirSector );
		PathStackSize.push_back( DirLength );

		DirSector = Extent;
		DirLength = Length;
	}
	else
	{
		PathStackExtent.push_back( JDirSector );
		PathStackSize.push_back( JDirLength );

		JDirSector = Extent;
		JDirLength = Length;
	}

	ParentSector = PathStackExtent.back();
	ParentLength = PathStackSize.back();
}

void ISO9660Directory::Pop()
{
	if ( !UsingJoliet )
	{
		DirSector = PathStackExtent.back();
		DirLength = PathStackSize.back();

		PathStackExtent.pop_back();
		PathStackSize.pop_back();
	}
	else
	{
		JDirSector = PathStackExtent.back();
		JDirLength = PathStackSize.back();

		PathStackExtent.pop_back();
		PathStackSize.pop_back();
	}

	if ( PathStackExtent.size() > 0 )
	{
		ParentSector = PathStackExtent.back();
		ParentLength = PathStackSize.back();
	}
	else
	{
		// We're in the root, so there is no parent
		ParentSector = DirSector;
		ParentLength = DirLength;
	}
}
