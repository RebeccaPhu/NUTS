#include "StdAfx.h"
#include "ISO9660Directory.h"
#include "NUTSError.h"
#include "libfuncs.h"
#include "ExtensionRegistry.h"
#include "ISOFuncs.h"

ISO9660Directory::ISO9660Directory( DataSource *pDataSource ) : Directory( pDataSource )
{
	pPriVolDesc = nullptr;
	pJolietDesc = nullptr;
}


ISO9660Directory::~ISO9660Directory(void)
{
}

int	ISO9660Directory::ReadDirectory(void)
{
	Files.clear();

	FileFOPData.clear();

	DWORD FileID = 0;

	QWORD DirOffset = DirSector;
	DWORD BOffset   = 0;
	DWORD DirCount  = 0;

	DirOffset *= pPriVolDesc->SectorSize;

	AutoBuffer Sector( pPriVolDesc->SectorSize );

	bool NeedRead = true;

	while ( DirCount < DirLength )
	{
		if ( NeedRead )
		{
			if ( pSource->ReadRaw( DirOffset, pPriVolDesc->SectorSize, (BYTE *) Sector ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		NeedRead = false;

		BYTE EntrySize = Sector[ BOffset + 0 ];

		if ( EntrySize == 0 )
		{
			// Sector not big enough to contain the entry. Move to next sector.
			DirOffset += pPriVolDesc->SectorSize;

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

			file.EncodingID = ENCODING_ASCII;
			file.Filename   = BYTEString( &Sector[ BOffset + 33 ], Sector[ BOffset + 32 ] );
			file.FSFileType = FT_Windows;
			file.Type       = FT_Arbitrary;
			file.Icon       = FT_Arbitrary;
			file.Length     = * (DWORD *) &Sector[ BOffset + 10 ];
			file.fileID     = FileID;

			file.Attributes[ ISOATTR_START_EXTENT ] = * (DWORD *) (BYTE *) &Sector[ BOffset +  2 ];
			file.Attributes[ ISOATTR_REVISION     ] = 0;
			file.Attributes[ ISOATTR_FORK_EXTENT  ] = 0;
			file.Attributes[ ISOATTR_FORK_LENGTH  ] = 0;

			// Do some filename mangling
			if ( ( Sector[ BOffset + 25 ] & 2 ) == 0 )
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

			// If we have a matching Associated File, we need to copy some details
			if ( rstrncmp( file.Filename, AssocFile.Filename, max( file.Filename.length(), AssocFile.Filename.length() ) ) )
			{
				// Note that only the most recent associated file survives
				file.Attributes[ ISOATTR_FORK_EXTENT ] = AssocFile.Attributes[ 0 ];
				file.Attributes[ ISOATTR_FORK_LENGTH ] = AssocFile.Length;

				file.ExtraForks = 1;
			}

			if ( Sector[ BOffset + 25 ] & 2 )
			{
				file.Flags |= FF_Directory;
				file.Type   = FT_Directory;
				file.Icon   = FT_Directory;
			}
			else if ( Sector[ BOffset + 25 ] & 4 )
			{
				// "Associated File" - this is basically a fork
				AssocFile = file;

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
				
						BYTE *SUSP      = (BYTE *) &Sector[ BOffset ];
						BYTE SUSPOffset = 33 + Sector[ BOffset + 32 ];

						if ( SUSPOffset & 1 ) { SUSPOffset++; }

						fop.pXAttr = &SUSP[ SUSPOffset ];
						fop.lXAttr = Sector[ BOffset ] - SUSPOffset;

						ProcessFOP( &fop );

						if ( fop.ReturnData.Identifier != L"" )
						{
							FileFOPData[ file.fileID ] = fop.ReturnData;
						}
					}
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

int	ISO9660Directory::WriteDirectory(void)
{
	return 0;
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