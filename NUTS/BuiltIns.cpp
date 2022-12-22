#include "StdAfx.h"
#include "BuiltIns.h"

#include "TextFileTranslator.h"
#include "HEXDumpTranslator.h"
#include "MODTranslator.h"
#include "libfuncs.h"

#include "ISO9660FileSystem.h"
#include "ISORawSectorSource.h"
#include "ZIPFile.h"
#include "DSKDataSource.h"

#include "NUTSFSTypes.h"
#include "Plugins.h"
#include "ROMDisk.h"

const ProviderIdentifier ISO_PROVIDER   = L"ISO_CDROM_Provider";
const ProviderIdentifier ZIP_PROVIDER   = L"ZIP_FILE_Provider";
const PluginIdentifier   BUILTIN_PLUGIN = L"NUTS_BuiltIn";
const WrapperIdentifier  CPCEMU_DSK     = L"CPCEMU_DSK_Wrapper";

BuiltIns::BuiltIns(void)
{
}


BuiltIns::~BuiltIns(void)
{
}

BuiltInTranslators BuiltIns::GetBuiltInTranslators( void )
{
	BuiltInTranslators tx;

	BuiltInTranslator t;

	t.TUID         = TUID_TEXT;
	t.TXFlags      = TXTextTranslator;
	t.FriendlyName = L"Text File";

	tx.push_back( t );

	t.TUID         = TUID_MOD_MUSIC;
	t.TXFlags      = TXAUDTranslator;
	t.FriendlyName = L"MOD Music File";

	tx.push_back( t );

	t.TUID         = TUID_HEX;
	t.TXFlags      = TXTextTranslator;
	t.FriendlyName = L"Hex Dump";

	tx.push_back( t );

	return tx;
}

void *BuiltIns::LoadTranslator( TXIdentifier TUID )
{
	void *pXlator = nullptr;

	if ( TUID == TUID_TEXT )
	{
		pXlator = new TextFileTranslator();
	}

	if ( TUID == TUID_MOD_MUSIC )
	{
		pXlator = new MODTranslator();
	}

	if ( TUID == TUID_HEX )
	{
		pXlator = new HEXDumpTranslator();
	}

	return pXlator;
}

BuiltInProviderList BuiltIns::GetBuiltInProviders()
{
	BuiltInProviderList pvl;

	NUTSProvider ZIPProvider;

	ZIPProvider.FriendlyName = L"PKWare";
	ZIPProvider.PluginID     = BUILTIN_PLUGIN;
	ZIPProvider.ProviderID   = ZIP_PROVIDER;

	pvl.push_back( ZIPProvider );

	NUTSProvider ISOProvider;

	ISOProvider.FriendlyName = L"CD/DVD";
	ISOProvider.PluginID     = BUILTIN_PLUGIN;
	ISOProvider.ProviderID   = ISO_PROVIDER;

	pvl.push_back( ISOProvider );

	return pvl;
}

FormatList BuiltIns::GetBuiltinFormatList( ProviderIdentifier PUID, bool ExcludeUnimageable )
{
	FormatList Formats;

	DWORD ISOFlags =
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_DynamicSize |
		FSF_Creates_Image | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_Uses_Extensions | FSF_NoDir_Extensions |
		FSF_Size |
		FSF_Accepts_Sidecars | FSF_Supports_FOP;

	DWORD ZIPFlags =
		FSF_Creates_Image | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_DynamicSize | 
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_Size |
		FSF_Uses_Extensions |
		FSF_Accepts_Sidecars | FSF_Supports_FOP | FSF_NoInPlaceAttrs;

	if ( PUID == ZIP_PROVIDER )
	{
		if ( ExcludeUnimageable )
		{
			return Formats;
		}

		FormatDesc Format;

		Format.Flags  = ZIPFlags;
		Format.FSID   = FSID_ZIP;
		Format.Format = L"ZIP File";

		Format.PreferredExtension = (BYTE *) "ZIP";

		Formats.push_back( Format );

		return Formats;
	}

	if ( PUID == ISO_PROVIDER )
	{
		FormatDesc Format;

		Format.Flags  = ISOFlags;
		Format.FSID   = FSID_ISOHS;
		Format.Format = L"High Sierra";

		Format.PreferredExtension = (BYTE *) "ISO";

		Formats.push_back( Format );

		Format.Flags  = ISOFlags;
		Format.FSID   = FSID_ISO9660;
		Format.Format = L"ISO 9660";

		Format.PreferredExtension = (BYTE *) "ISO";

		Formats.push_back( Format );

		return Formats;
	}

	return Formats; // Empty list
}

FSHints BuiltIns::GetOffers( DataSource *pSource, NativeFile *pFile )
{
	FSHints hints;

	{ // ZIP files
		FSHint hint = { FSID_ZIP, 0, WID_Null };

		BYTE Buf[ 4 ];

		pSource->ReadRaw( 0, 4, Buf );

		if ( rstrncmp( Buf, (BYTE *) "PK", 2 ) )
		{
			hint.Confidence += 20;
		}

		if ( ( pFile->Flags & FF_Extension ) && ( rstrncmp( pFile->Extension, (BYTE *) "ZIP", 3 ) ) )
		{
			hint.Confidence += 10;
		}

		hints.push_back( hint );
	}

	{ // ISO 9660
		FSHint hint = { FSID_ISO9660, 0 };

		BYTE Buf[ 5 ];

		pSource->ReadRaw( 0x8001, 5, Buf );

		if ( rstrncmp( Buf, (BYTE *) "CD001", 5 ) )
		{
			hint.Confidence += 20;
		}

		if ( ( pFile->Flags & FF_Extension ) && ( rstrncmp( pFile->Extension, (BYTE *) "ISO", 3 ) ) )
		{
			hint.Confidence += 10;
		}

		hints.push_back( hint );
	}

	{ // ISO High Sierra
		FSHint hint = { FSID_ISOHS, 0 };

		BYTE Buf[ 5 ];

		pSource->ReadRaw( 0x8009, 5, Buf );

		if ( rstrncmp( Buf, (BYTE *) "CDROM", 5 ) )
		{
			hint.Confidence += 20;
		}

		if ( ( pFile->Flags & FF_Extension ) && ( rstrncmp( pFile->Extension, (BYTE *) "ISO", 3 ) ) )
		{
			hint.Confidence += 10;
		}

		hints.push_back( hint );
	}

	return hints;
}

FileSystem *BuiltIns::LoadFS( FSIdentifier FSID, DataSource *pSource )
{
	FileSystem *pFS = nullptr;

	if ( FSID == FSID_ZIP )
	{
		pFS = new ZIPFile( pSource );
	}
	else if ( ( FSID == FSID_ISO9660 ) || ( FSID == FSID_ISOHS ) )
	{
		// Do a raw test
		const BYTE SectorHeader[ 12 ] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		BYTE HeaderCheck[ 12 ];

		if ( pSource->ReadRaw( 0, 12, HeaderCheck ) == DS_SUCCESS )
		{
			if ( memcmp( HeaderCheck, SectorHeader, 12 ) == 0 )
			{
				// Looks raw. Wrap this up.
				ISORawSectorSource *pISOSource = new ISORawSectorSource( pSource );

				pSource = pISOSource;
			}
		}

		pFS = new ISO9660FileSystem( pSource );

		pFS->FSID = FSID;
	}

	return pFS;
}

std::wstring BuiltIns::ProviderName( ProviderIdentifier PRID )
{
	if( PRID == ZIP_PROVIDER )
	{
		return L"PKWare";
	}
	else if ( PRID == FS_Windows )
	{
		return L"Microsoft";
	}
	else if ( PRID == FS_Root )
	{
		return L"NUTS";
	}
	else if ( PRID == ISO_PROVIDER )
	{
		return L"CD/DVD";
	}

	return L"";
}

std::wstring BuiltIns::FSName( FSIdentifier FSID )
{
	if( FSID == FSID_ZIP )
	{
		return L"ZIP File";
	}
	else if ( FSID == FS_Windows )
	{
		return L"Windows Drive";
	}
	else if ( FSID == FS_Root )
	{
		return L"System";
	}
	else if ( FSID == FSID_ISO9660 )
	{
		return L"ISO9660 CD-ROM";
	}
	else if ( FSID == FSID_ISOHS )
	{
		return L"High Sierra CD-ROM";
	}
	else if ( FSID == FSID_ROMDisk )
	{
		return L"ROM Disk Scratch Pad";
	}

	return L"";
}

std::wstring BuiltIns::FSExt( FSIdentifier FSID )
{
	if( FSID == FSID_ZIP )
	{
		return L"ZIP";
	}
	else if ( FSID == FSID_ISO9660 )
	{
		return L"ISO";
	}
	else if ( FSID == FSID_ISOHS )
	{
		return L"ISO";
	}

	return L"";
}

BuiltInMenuList BuiltIns::GetBuiltInMenuList()
{
	BuiltInMenuList fsmenu;

	/* Add ZIP Files manually */
	FormatMenu format;
	FSMenu     menu;

	menu.Provider = L"PKWare";
	menu.FS.clear();

	format.FS = L"ZIP File";
	format.ID = FSID_ZIP;

	menu.FS.push_back( format );
	fsmenu.push_back( menu );

	menu.Provider = L"CD/DVD";
	menu.FS.clear();

	format.FS = L"High Sierra CD-ROM";
	format.ID = FSID_ISOHS;

	menu.FS.push_back( format );

	format.FS = L"ISO9660 CD-ROM";
	format.ID = FSID_ISO9660;

	menu.FS.push_back( format );

	fsmenu.push_back( menu );

	return fsmenu;
}

WrapperList BuiltIns::GetWrappers()
{
	WrapperList wrappers;

	WrapperDescriptor dsk;

	dsk.FriendlyName = L"CPCEMU DSK Format";
	dsk.Identifier   = CPCEMU_DSK;

	wrappers.push_back( dsk );

	return wrappers;
}

DataSource *BuiltIns::GetWrapper( WrapperIdentifier wrapper, DataSource *pSource )
{
	if ( wrapper == CPCEMU_DSK )
	{
		return new DSKDataSource( pSource );
	}

	return nullptr;
}

BuiltIns NUTSBuiltIns;