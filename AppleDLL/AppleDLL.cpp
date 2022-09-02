#include "stdafx.h"

#include "..\NUTS\EncodingEdit.h"

#include "AppleDLL.h"

#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/TZXIcons.h"
#include "AppleDefs.h"
#include "../NUTS/Preference.h"
#include "MacIcon.h"
#include "resource.h"

#include "MacintoshMFSFileSystem.h"

#include <MMSystem.h>

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

const FTIdentifier       FILE_MACINTOSH     = L"MacintoshFileObject";
const EncodingIdentifier ENCODING_MACINTOSH = L"MacintoshEncoding";
const PluginIdentifier   APPLE_PLUGINID     = L"AppleNUTSPlugin";
const ProviderIdentifier APPLE_PROVIDER     = L"Apple_Provider";
const FontIdentifier     CHICAGO            = L"Chicago_Font";

BYTE *pChicago = nullptr;

FSDescriptor AppleFS[] = {
	{
		/* .FriendlyName = */ L"Macintosh MFS 400K",
		/* .PUID         = */ FSID_MFS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors,
		512, 80 * 10 * 512,
		(BYTE *) "IMG"
	},
	{
		/* .FriendlyName = */ L"Macintosh MFS Hard Disk",
		/* .PUID         = */ FSID_MFS_HD,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		512, 0,
		(BYTE *) "IMG"
	},
};

#define APPLEFS_COUNT ( sizeof(AppleFS) / sizeof(FSDescriptor) )

std::wstring ImageExtensions[] = { L"DSK", L"IMG" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )


NUTSProvider ProviderAmstrad = { L"Apple", APPLE_PLUGINID, APPLE_PROVIDER };

WCHAR *pChicagoName = L"Chicago";

void *CreateFS( FSIdentifier FSID, DataSource *pSource )
{
	void *pFS = NULL;

	if ( ( FSID == FSID_MFS ) || ( FSID == FSID_MFS_HD ) )
	{
		pFS = (void *) new MacintoshMFSFileSystem( pSource );
	}

	return pFS;
}

void LoadFonts()
{
	if ( pChicago == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_CHICAGO ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pChicago = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pChicago, 256 * 8 );
		memcpy( pChicago, lpAddress, dwSize );
	}
}

bool TranslateISOContent( FOPData *fop )
{
	NativeFile *pFile = (NativeFile *) fop->pFile;

	if ( ( fop->Direction == FOP_ReadEntry ) && ( pFile->FSFileType == FT_NULL ) )
	{
		// The data block is basically Rock Ridge, but we only care about BA/AA and NM,
		// the latter since Apple filenames are more expressive than the DChar limitation of ISO9660/High Sierra.
		DWORD lXAttr = fop->lXAttr;
		DWORD s = 0;

		BYTEString NM = pFile->Filename;
		DWORD FT_FCC = 0x20202020;
		DWORD CT_FCC = 0x20202020;
		bool  HaveAppleBlock = false;
		bool  HaveNewerBlock = false;

		while ( s< lXAttr )
		{
			if ( ( s + 3 ) > lXAttr )
			{
				break;
			}

			BYTE DLen = fop->pXAttr[ s + 2 ];

			if ( DLen == 0U )
			{
				// Eh?

				break;
			}

			if (  ( fop->pXAttr[ s ] == 'B' ) && ( fop->pXAttr[ s + 1 ] == 'A' ) )
			{
				// The BA block is malformed. DLen is unreliable.
				if ( fop->pXAttr[ s + 2 ] == 0x01 )
				{
					DLen = 6; // ProDOS block. Not used, but must skip it.
				}

				if ( fop->pXAttr[ s + 2 ] == 0x02 )
				{
					DLen = 13; // Mac block
				}
			}

			if ( ( s + DLen ) > lXAttr )
			{
				break;
			}

			if ( ( fop->pXAttr[ s ] == 'N' ) && ( fop->pXAttr[ s + 1 ] == 'M' ) )
			{
				// Alternate name field.
				NM = BYTEString( &fop->pXAttr[ s + 5 ], DLen - 5 );
			}

			if (  ( fop->pXAttr[ s ] == 'B' ) && ( fop->pXAttr[ s + 1 ] == 'A' ) )
			{
				// Make sure not a ProDOS entry (yes, Apple technically supported ProDOS on CDs).
				if ( !HaveNewerBlock )
				{
					if ( fop->pXAttr[ s + 2 ] == 0x06 )
					{
						FT_FCC = BEDWORD( &fop->pXAttr[ s + 3 ] );
						CT_FCC = BEDWORD( &fop->pXAttr[ s + 7 ] );

						// Skip finder flags.
						HaveAppleBlock = true;
					}
				}
			}

			if (  ( fop->pXAttr[ s ] == 'A' ) && ( fop->pXAttr[ s + 1 ] == 'A' ) )
			{
				// New style block - Rock Ridge compliant
				if ( fop->pXAttr[ s + 3 ] == 0x02 )
				{
					FT_FCC = BEDWORD( &fop->pXAttr[ s + 4 ] );
					CT_FCC = BEDWORD( &fop->pXAttr[ s + 8 ] );

					// Skip finder flags.
					HaveAppleBlock = true;
				}

				HaveNewerBlock = true;
			}

			s += DLen;
		}

		if ( HaveAppleBlock )
		{
			pFile->Attributes[ 11 ] = FT_FCC;
			pFile->Attributes[ 12 ] = CT_FCC;
			pFile->FSFileType = FILE_MACINTOSH;
			pFile->EncodingID = ENCODING_MACINTOSH;
			pFile->Filename   = NM;

			// Process FT/CT string
			BYTE FTCT[ 10 ] = { 0,0,0,0,0,0,0,0,0,0 };

			WLEDWORD( &FTCT[ 0 ], FT_FCC );
			WLEDWORD( &FTCT[ 5 ], CT_FCC );

			rsprintf( fop->ReturnData.Descriptor, "Type: %s, Creator: %s", &FTCT[ 0 ], &FTCT[ 5 ] );

			fop->ReturnData.Identifier = L"Apple Macintosh File";

			if ( (bool) Preference( L"UseResolvedIcons" ) )
			{
				if ( pFile->ExtraForks > 0 )
				{
					FileSystem *pFS = (FileSystem *) fop->pFS;

					CTempFile obj;

					pFS->ReadFork( pFile->fileID, 0, obj );

					ExtractIcon( pFile, obj, pFS->pDirectory );
				}
			}

			return true;
		}
	}

	if ( ( fop->Direction == FOP_WriteEntry ) && ( pFile->FSFileType == FILE_MACINTOSH ) )
	{
		fop->lXAttr = 14 + 5 + pFile->Filename.length();

		fop->pXAttr[ 0 ] = 'A';
		fop->pXAttr[ 1 ] = 'A';
		fop->pXAttr[ 2 ] = 0x0E;
		fop->pXAttr[ 3 ] = 0x02; // Mac type
		WBEDWORD( &fop->pXAttr[ 4 ], pFile->Attributes[ 11 ] );
		WBEDWORD( &fop->pXAttr[ 8 ], pFile->Attributes[ 12 ] );
		WBEWORD(  &fop->pXAttr[ 0x0C ], 0x0100 ); // Finder flags - defaults.

		// Write an NM entry.
		fop->pXAttr[ 0x0E ] = 'N';
		fop->pXAttr[ 0x0F ] = 'M';
		fop->pXAttr[ 0x10 ] = 5 + pFile->Filename.length();
		fop->pXAttr[ 0x11 ] = 1; // Version
		fop->pXAttr[ 0x12 ] = 0; // Flags

		memcpy( &fop->pXAttr[ 0x13 ], (BYTE *) pFile->Filename, pFile->Filename.length() );

		bool SidecarsAnyway = (bool) Preference( L"SidecarsAnyway", false );

		if ( !SidecarsAnyway )
		{
			pFile->Flags |= FF_AvoidSidecar;
		}

		return true;
	}

	if ( ( fop->Direction == FOP_SetDirType ) && ( pFile->FSFileType == FILE_MACINTOSH ) )
	{
		pFile->Attributes[ 11 ] = MAKEFOURCC( ' ', 'R', 'I', 'D' );
		pFile->Attributes[ 12 ] = MAKEFOURCC( 'S', 'T', 'U', 'N' );

		return true;
	}

	return false;
}

bool TranslateZIPContent( FOPData *fop )
{
	if ( fop->Direction == FOP_PostRead )
	{
		FileSystem *pFS = (FileSystem *) fop->pFS;

		for ( int i=0; i<pFS->pDirectory->Files.size(); i++ )
		{
			NativeFile *pFile = &pFS->pDirectory->Files[ i ];

			if ( pFile->FSFileType == FT_NULL )
			{
				// Go looking for a sidecar
				BYTEString SidecarFilename;
				BYTEString sidecarF = BYTEString( pFile->Filename.size() + 2 );

				sidecarF[ 0 ] = (BYTE) '.';
				sidecarF[ 1 ] = (BYTE) '_';

				memcpy( &sidecarF[ 2 ], (BYTE *) pFile->Filename, pFile->Filename.length() );

				SidecarFilename = BYTEString( (BYTE *) sidecarF, sidecarF.length() );

				// Now see if such a thing exists
				for ( int j=0; j<pFS->pDirectory->Files.size(); j++ )
				{
					if ( i == j ) { continue; }

					NativeFile *pSidecarFile = &pFS->pDirectory->Files[ j ];

					if ( !rstricmp( pSidecarFile->Filename, SidecarFilename ) )
					{
						continue;
					}

					CTempFile obj;

					pFS->ReadFile( j, obj );

					BYTE Data[ 16 ];

					obj.Seek( 0 );
					obj.Read( Data, 4 );

					if ( BEDWORD( Data ) != 0x00051607 )
					{
						return false;
					}

					obj.Read( Data, 4 );

					if ( BEDWORD( Data ) != 0x00020000 )
					{
						return false;
					}

					obj.Read( Data, 16 );

					// Skip the Home FS. It seems it's not reliably set anyway.

					WORD forks;
					DWORD Offset1 = 0;
					DWORD Offset2 = 0;
					DWORD Length1 = 0;
					DWORD Length2 = 0;

					obj.Read( Data, 2 );

					forks = BEWORD( Data );

					for ( WORD i=0; i<forks; i++ )
					{
						DWORD Type;
						DWORD Offset;
						DWORD Length;

						obj.Read( Data, 4 ); Type   = BEDWORD( Data );
						obj.Read( Data, 4 ); Offset = BEDWORD( Data );
						obj.Read( Data, 4 ); Length = BEDWORD( Data );

						if ( Type == 0x00000009 )
						{
							Offset1 = Offset;
							Length1 = Length;
						}

						if ( Type == 0x00000002 )
						{
							Offset2 = Offset;
							Length2 = Length;
						}
					}
		
					if ( Offset2 != 0 )
					{
						CTempFile ResourceFork;

						BYTE Buffer[ 0x200 ];

						DWORD BytesToGo = Length2;

						Length2 = min( Length2, ( obj.Ext() - Offset2 ) );
		
						obj.Seek( Offset2 );
						ResourceFork.Seek( 0 );

						while ( BytesToGo > 0 )
						{
							obj.Read( Buffer, min( BytesToGo, 0x200 ) );
							ResourceFork.Write( Buffer, min( BytesToGo, 0x200 ) );

							BytesToGo -= min( BytesToGo, 0x200 );
						}

						ExtractIcon( pFile, ResourceFork, pFS->pDirectory );
					}

					return true;
				}
			}
		}
	}

	return false;
}

UINT APPL_ICON;
UINT DOC_ICON;
UINT BLANK_ICON;

static WCHAR *PluginAuthor = L"Rebecca Gellman";

APPLEDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

		cmd->OutParams[ 0 ].Value = 0x00000001;
		cmd->OutParams[ 1 ].pPtr  = (void *) APPLE_PLUGINID.c_str();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderAmstrad;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = APPLEFS_COUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &AppleFS[ cmd->InParams[ 1 ].Value ];
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportImageExtensions:
		cmd->OutParams[ 0 ].Value = IMAGE_EXT_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetImageExtension:
		cmd->OutParams[ 0 ].pPtr = (void *) ImageExtensions[ cmd->InParams[ 0 ].Value ].c_str();
		cmd->OutParams[ 1 ].Value = FT_MiscImage;
		cmd->OutParams[ 2 ].Value = FT_DiskImage;

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateFileSystem:
		{
			DataSource *pSource = (DataSource *) cmd->InParams[ 2 ].pPtr;

			void *pFS = (void *) CreateFS( FSIdentifier( (WCHAR *) cmd->InParams[ 0 ].pPtr ), pSource );

			cmd->OutParams[ 0 ].pPtr = pFS;

			if ( pFS == nullptr )
			{
				return NUTS_PLUGIN_ERROR;
			}

			return NUTS_PLUGIN_SUCCESS;
		}

	case PC_ReportEncodingCount:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) pChicago;
			cmd->OutParams[ 1 ].pPtr = (void *) pChicagoName;
			cmd->OutParams[ 2 ].pPtr = (void *) CHICAGO.c_str();
			cmd->OutParams[ 3 ].pPtr = (void *) ENCODING_MACINTOSH.c_str();
			cmd->OutParams[ 4 ].pPtr = nullptr;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportIconCount:
		cmd->OutParams[ 0 ].Value = 3;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeIcon:
		{
			BYTE Icon   = (BYTE) cmd->InParams[ 0 ].Value;

			if ( Icon == 0 )
			{
				APPL_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_APPL ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}

			if ( Icon == 1 )
			{
				DOC_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_DOC ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}

			if ( Icon == 2 )
			{
				BLANK_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_BLANK ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_PLUGINBANNER ) );
		cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
		cmd->OutParams[ 2 ].Value = 5;
		cmd->OutParams[ 3 ].Value = 0;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetIconLicensing:
		{
			static WCHAR *pIcons[5] =
			{
				L"Apple Logo", L"Macintosh Application Icon", L"Macintosh Document Icon", L"Macintosh File Icon", L"Repair Icon"
			};

			static int iIcons[ 5 ] = { IDB_APPLE, IDB_APPL, IDB_DOC, IDB_BLANK, IDB_REPAIR };
			
			static WCHAR *pIconset = L"Large Android Icons";
			static WCHAR *pAuthor  = L"Aha-Soft";
			static WCHAR *pLicense = L"CC Attribution 3.0 US";
			static WCHAR *pURL     = L"https://creativecommons.org/licenses/by/3.0/us/";
			static WCHAR *pEmpty   = L"";
			static WCHAR *pSet2    = L"Company Logo or System Icon";
			static WCHAR *pApple   = L"Apple Computer Inc.";
			static WCHAR *pFair    = L"Icons and logos used under fair use for familiar purposes";

			cmd->OutParams[ 0 ].pPtr = pIcons[ cmd->InParams[ 0 ].Value ];
			cmd->OutParams[ 1 ].pPtr = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( iIcons[ cmd->InParams[ 0 ].Value ] ) );
			
			if ( cmd->InParams[ 0 ].Value == 4 )
			{
				cmd->OutParams[ 2 ].pPtr = pIconset;
				cmd->OutParams[ 3 ].pPtr = pAuthor;
				cmd->OutParams[ 4 ].pPtr = pLicense;
				cmd->OutParams[ 5 ].pPtr = pURL;
			}
			else
			{
				cmd->OutParams[ 2 ].pPtr = pSet2;
				cmd->OutParams[ 3 ].pPtr = pApple;
				cmd->OutParams[ 4 ].pPtr = pFair;
				cmd->OutParams[ 5 ].pPtr = pEmpty;
			}
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFOPDirectoryTypes:
		{
			static FOPDirectoryType FT;

			FT.FriendlyName = L"Apple";
			FT.Identifier   = FILE_MACINTOSH;

			cmd->OutParams[ 0 ].pPtr = &FT;
			cmd->OutParams[ 1 ].pPtr = nullptr;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_TranslateFOPContent:
		{
			FOPData *fop = (FOPData *) cmd->InParams[ 0 ].pPtr;

			if ( fop->DataType == FOP_DATATYPE_CDISO )
			{
				bool r= TranslateISOContent( fop );

				if ( r ) { cmd->OutParams[ 0 ].Value = 0xFFFFFFFF; } else { cmd->OutParams[ 0 ].Value = 0x00000000; }

				return NUTS_PLUGIN_SUCCESS;
			}

			if ( fop->DataType == FOP_DATATYPE_ZIPATTR )
			{
				bool r= TranslateZIPContent( fop );

				if ( r ) { cmd->OutParams[ 0 ].Value = 0xFFFFFFFF; } else { cmd->OutParams[ 0 ].Value = 0x00000000; }

				return NUTS_PLUGIN_SUCCESS;
			}
		}
		
		break;
	}

	return NUTS_PLUGIN_UNRECOGNISED;
};