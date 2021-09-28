#include "stdafx.h"

#include "Plugins.h"
#include "resource.h"

#include "IDE8Source.h"
#include "ImageDataSource.h"
#include "OffsetDataSource.h"
#include "TextFileTranslator.h"

#include "BitmapCache.h"
#include "ExtensionRegistry.h"
#include "DataSourceCollector.h"
#include "DSKDataSource.h"

#include "ZIPFile.h"

#include <string>



extern DataSourceCollector *pCollector;

CPlugins FSPlugins;

CPlugins::CPlugins()
{
	Plugins.clear();

	pPC437Font = nullptr;

	FontNames[ FONTID_PC437 ] = L"PC437";
}

CPlugins::~CPlugins()
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		FreeLibrary( iter->Handle );

		iter++;
	}

	Plugins.clear();

	if ( pPC437Font != nullptr )
	{
		free( pPC437Font );
	}
}

void CPlugins::LoadPlugins()
{
	EncodingFontMap[ ENCODING_ASCII ] = std::vector<DWORD>();
	EncodingFontMap[ ENCODING_ASCII ].push_back( FONTID_PC437 );

	EncodingFontSelectors[ 0 ][ ENCODING_ASCII ] = 0;
	EncodingFontSelectors[ 1 ][ ENCODING_ASCII ] = 0;

	PluginID   = 0x0001;
	EncodingID = 0x01000000;
	FontID     = 0x01000000;
	IconID     = 0x01000000;
	FSFTID     = 0x01000000;
	TXID       = 0x00000001;

	WIN32_FIND_DATA wfd;

	HANDLE hFind = FindFirstFile( L"*.dll", &wfd );

	if ( hFind != INVALID_HANDLE_VALUE )
	{
		BOOL Files = TRUE;

		while ( Files )
		{
			LoadPlugin( wfd.cFileName );

			Files = FindNextFile( hFind, &wfd );
		}

		FindClose( hFind );
	}
}

void CPlugins::LoadPlugin( WCHAR *plugin )
{
	BYTE *sig = (BYTE *) "NUTS Plugin Signature";

	HMODULE hModule = LoadLibrary( plugin );

	if ( hModule != NULL )
	{
		NUTSPlugin plugin;

		BYTE **ppSig = (BYTE **) GetProcAddress( hModule, "NUTSSignature" );

		if ( ( ppSig != nullptr ) && ( *ppSig != nullptr ) )
		{
			if ( !rstricmp( *ppSig, sig ) )
			{
				FreeLibrary( hModule );

				return;
			}
		}
		else
		{
			FreeLibrary( hModule );

			return;
		}

		plugin.Handle         = hModule;
		plugin.PluginID       = PluginID;
		plugin.CommandHandler = (fnNUTSPluginFunction)   GetProcAddress( hModule, "NUTSCommandHandler" );

		if ( plugin.CommandHandler == nullptr )
		{
			FreeLibrary( hModule );

			return;
		}

		PluginCommand cmd;

		/* Initialise the plugin and set the connector points */
		cmd.CommandID = PC_SetPluginConnectors;
		cmd.InParams[ 0 ].pPtr = (void *) pCollector;
		cmd.InParams[ 1 ].pPtr = (void *) pGlobalError;

		if ( plugin.CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
		{
			FreeLibrary( hModule );

			return;
		}

		Plugins.push_back( plugin );

		/* Get the provider count */
		cmd.CommandID = PC_ReportProviders;

		plugin.CommandHandler( &cmd );

		BYTE NumProviders = (BYTE) cmd.OutParams[ 0 ].Value;

		for ( BYTE pid = 0; pid<NumProviders; pid++ )
		{
			/* Get Provider Descriptor */
			cmd.CommandID = PC_GetProviderDescriptor;
			cmd.InParams[ 0 ].Value = pid;

			if ( plugin.CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
			{
				pid++;

				continue;
			}

			NUTSProvider p = * (NUTSProvider *) cmd.OutParams[ 0 ].pPtr;
			
			p.PluginID   = PluginID;
			p.ProviderID = MAKEPROVID( PluginID, pid );

			Providers.push_back( p );

			/* Get Filesystems and load them */
			cmd.CommandID = PC_ReportFileSystems;
			cmd.InParams[ 0 ].Value = pid;

			if ( plugin.CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
			{
				pid++;

				continue;
			}

			BYTE NumFS = (BYTE) cmd.OutParams[ 0 ].Value;

			for ( BYTE fsid = 0; fsid<NumFS; fsid++ )
			{
				GetFileSystemDetails( &plugin, pid, fsid );
			}

		}

		/* Set the FS FileTypes */
		cmd.CommandID = PC_ReportFSFileTypeCount;

		if ( plugin.CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			DWORD FSFTs = cmd.OutParams[ 0 ].Value;

			cmd.CommandID           = PC_SetFSFileTypeBase;
			cmd.InParams[ 0 ].Value = FSFTID;

			if ( plugin.CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				FSFTID += FSFTs;
			}
		}

		LoadImageExtensions( &plugin );
		LoadFonts( &plugin );
		LoadIcons( &plugin );
		LoadTranslators( &plugin );
		LoadRootHooks( &plugin );
		LoadRootCommands( &plugin );

		PluginID++;
	}
}

void CPlugins::GetFileSystemDetails( NUTSPlugin *plugin, BYTE pid, BYTE fsid )
{
	PluginCommand cmd;

	cmd.CommandID = PC_DescribeFileSystem;
	cmd.InParams[ 0 ].Value = pid;
	cmd.InParams[ 1 ].Value = fsid;

	if ( plugin->CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
	{
		return;
	}

	FSDescriptor fs = * ( FSDescriptor *) cmd.OutParams[ 0 ].pPtr;

	fs.PUID = MAKEFSID( PluginID, pid, fsid );

	FSDescriptors.push_back( fs );

	cmd.CommandID = PC_GetOffsetLists;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		BYTE oc = cmd.OutParams[ 0 ].Value;
		
		if ( oc > 0 )
		{
			std::vector<QWORD> ol;

			for ( BYTE o=0; o<oc; o++ )
			{
				DWORD ofs = cmd.OutParams[ 1 + o ].Value;

				ol.push_back( ofs );
			}

			ImageOffsets[ fs.PUID ] = ol;
		}
	}
}

NUTSProviderList CPlugins::GetProviders( void )
{
	return Providers;
}

FSDescriptorList CPlugins::GetFilesystems( DWORD ProviderID )
{
	FSDescriptorList l;

	FSDescriptor_iter i;

	for ( i = FSDescriptors.begin(); i != FSDescriptors.end(); i++ )
	{
		if ( PROVIDERID( i->PUID ) == PROVIDERID (ProviderID ) )
		{
			if ( PLUGINID( i->PUID ) == PLUGINID( ProviderID ) )
			{
				l.push_back( *i );
			}
		}
	}

	return l;
}

FormatList CPlugins::GetFormats( DWORD PUID )
{
	FormatList Formats;

	if ( PUID == PUID_ZIP )
	{
		FormatDesc Format;

		Format.Flags  = FSF_DynamicSize | FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw;
		Format.FUID   = PUID_ZIP;
		Format.Format = L"ZIP File";
		Formats.push_back( Format );

		return Formats;
	}

	FSDescriptor_iter iFS;

	for ( iFS = FSDescriptors.begin(); iFS != FSDescriptors.end(); iFS++ )
	{
		if ( PLUGINID( iFS->PUID ) == PUID )
		{
			FormatDesc Format;

			Format.Format     = iFS->FriendlyName;
			Format.FUID       = iFS->PUID;
			Format.Flags      = iFS->Flags;
			Format.MaxSize    = iFS->MaxSize;
			Format.SectorSize = iFS->SectorSize;

			Formats.push_back( Format );
		}
	}

	return Formats;
}

TranslatorList CPlugins::GetTranslators( DWORD PUID, DWORD Type )
{
	TranslatorList Xlators;

	TranslatorIterator iter;

	for ( iter = Translators.begin(); iter != Translators.end(); iter++ )
	{
		if ( ( iter->Flags & Type ) || ( PUID == NULL ) )
		{
			if ( (PROVID( iter->ProviderID) == PUID ) || ( PUID == NULL ) )
			{
				Xlators.push_back( *iter );
			}
		}
	}

	return Xlators;
}

FSHints CPlugins::FindFS( DataSource *pSource, NativeFile *pFile )
{
	std::vector<FSHint> hints;
	
	FSDescriptor_iter iter = FSDescriptors.begin();

	while ( iter != FSDescriptors.end() )
	{
		FSHint hint  = { 0, 0 };

		FileSystem *pFS = LoadFS( iter->PUID, pSource );

		if ( pFS == nullptr )
		{
			iter++;

			continue;
		}

		pFS->FSID = iter->PUID;

		if ( pFile == nullptr )
		{
			hint = pFS->Offer( nullptr );
		}
		else
		{
			hint = pFS->Offer( pFile->Extension );
		}

		hint.FSID = iter->PUID;

		if ( hint.Confidence > 0 )
		{
			hints.push_back( hint );
		}

		iter++;

		delete pFS;

		pCollector->ReleaseSources();
	}

	FSHint hint = { 0, 0 };

	hint.FSID      = FSID_ZIP;

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

	DataSourceCollector *px = pCollector;

	return hints;
}

FileSystem *CPlugins::FindAndLoadFS( DataSource *pSource, NativeFile *pFile )
{
	FSHints hints = FindFS( pSource, pFile );

	WORD Confidence = 0;
	DWORD FSID      = FS_Null;
	
	FileSystem *newFS = nullptr;

	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		if ( iHint->Confidence > Confidence )
		{
			Confidence = iHint->Confidence;
			FSID       = iHint->FSID;
		}
	}

	if ( FSID != FS_Null )
	{
		newFS = LoadFS( FSID, pSource );
	}

	return newFS;
}

FileSystem *CPlugins::LoadFS( DWORD FSID, DataSource *pSource )
{
	if ( FSID == FSID_ZIP )
	{
		FileSystem *pFS = new ZIPFile( pSource );

		return pFS;
	}
	
	FSDescriptor_iter iter = FSDescriptors.begin();

	DataSource *pAuxSource   = nullptr;
	DataSource *pCloneSource = pSource;

	while ( iter != FSDescriptors.end() )
	{
		if ( iter->PUID == FSID )
		{
			std::vector<QWORD> Offsets;

			if ( ImageOffsets.find( FSID ) != ImageOffsets.end() )
			{
				Offsets = ImageOffsets[ FSID ];
			}
			else
			{
				Offsets.push_back( 0 );
			}

			/* Try all available offsets */
			int Attempts = 0;

			FSHint hint, rhint;

			hint.Confidence = 0;

			FileSystem *fav = nullptr;
			FileSystem *pFS = nullptr;

			std::vector<QWORD>::iterator iOffset = Offsets.begin();

			while ( iOffset != Offsets.end() )
			{
				QWORD Offset = *iOffset;

				if ( Offset != 0 )
				{
					pAuxSource = new OffsetDataSource( (DWORD) Offset, pSource );

					pCloneSource = pAuxSource;
				}
				else
				{
					/* Need a DSK? */
					if ( iter->Flags & FSF_Uses_DSK )
					{
						/* Need a DSK, so create a source and wrap it around */
						pAuxSource = new DSKDataSource( pSource );

						pCloneSource = pAuxSource;
					}
				}

				pFS = nullptr;
				
				NUTSPlugin *plugin = GetPlugin( FSID );

				if ( plugin != nullptr )
				{
					PluginCommand cmd;

					cmd.CommandID = PC_CreateFileSystem;
					cmd.InParams[ 0 ].Value = PROVIDERID( FSID );
					cmd.InParams[ 1 ].Value = FSIDID( FSID );
					cmd.InParams[ 2 ].pPtr  = (void *) pCloneSource;
				
					if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
					{
						pFS = (FileSystem *) cmd.OutParams[ 0 ].pPtr;

						OutputDebugStringA( "Created " );
						OutputDebugString( FSName( FSID ).c_str() );
						OutputDebugStringA( "\r\n" );
					}
				}

				if ( pFS == nullptr )
				{
					break;
				}

				pFS->FSID = FSID;
				pFS->PLID = PLUGINID( FSID );

				if ( pAuxSource != nullptr )
				{
					pAuxSource->Release();
				}

				rhint = pFS->Offer( nullptr );

				if ( rhint.Confidence >= hint.Confidence )
				{
					if ( fav != nullptr )
					{
						delete fav;
					}

					fav = pFS;

					hint.Confidence = rhint.Confidence;
				}
				else
				{
					delete pFS;
				}

				iOffset++;
			}

			pFS = fav;

			return pFS;
		}

		for ( RootHookIterator i = RootHooks.begin(); i != RootHooks.end(); i++ )
		{
			if ( i->HookFSID == FSID )
			{
				NUTSPlugin *p = GetPlugin( i->HookFSID );

				PluginCommand cmd;

				cmd.CommandID = PC_CreateFileSystem;
				cmd.InParams[ 0 ].Value = PROVIDERID( FSID );
				cmd.InParams[ 1 ].Value = FSIDID( FSID );
				cmd.InParams[ 2 ].pPtr  = (void *) pSource;

				if ( p->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
				{
					FileSystem *pFS = (FileSystem *) cmd.OutParams[ 0 ].pPtr;

					return pFS;
				}
			}
		}

		iter++;
	}

	return nullptr;
}

NUTSPlugin *CPlugins::GetPlugin( DWORD FSID )
{
	for ( Plugin_iter i = Plugins.begin(); i != Plugins.end(); i++ )
	{
		if ( i->PluginID == PLUGINID( FSID ) )
		{
			return &*i;
		}
	}

	return nullptr;
}

std::wstring CPlugins::FSName( DWORD FSID )
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

	std::wstring name = L"Unknown File System";

	FSDescriptor_iter fi;

	for ( fi = FSDescriptors.begin(); fi != FSDescriptors.end(); fi++ )
	{
		if ( fi->PUID ==FSID )
		{
			name = fi->FriendlyName;

			return name;
		}
	}

	return name;
}

WCHAR *CPlugins::FontName( DWORD ReqFontID )
{
	static WCHAR *PC437Name = L"PC437";

	if ( ReqFontID == FONTID_PC437 )
	{
		return PC437Name;
	}

	if ( FontNames.find( ReqFontID ) != FontNames.end() )
	{
		return (WCHAR *) FontNames[ ReqFontID ].c_str();
	}

	return PC437Name;
}

void *CPlugins::LoadFont( DWORD ReqFontID )
{
	if ( ReqFontID == FONTID_PC437 )
	{
		if ( pPC437Font == nullptr )
		{
			HMODULE hModule  = GetModuleHandle(NULL);
			HRSRC hResource  = FindResource(hModule, MAKEINTRESOURCE( IDF_PC437 ), RT_RCDATA);
			HGLOBAL hMemory  = LoadResource(hModule, hResource);
			DWORD dwSize     = SizeofResource(hModule, hResource);
			LPVOID lpAddress = LockResource(hMemory);

			pPC437Font = malloc( (size_t) dwSize );

			memcpy( pPC437Font, lpAddress, dwSize );
		}

		return pPC437Font;
	}

	if ( FontList.find( ReqFontID ) != FontList.end() )
	{
		return FontList[ ReqFontID ];
	}

	return nullptr;
}

DWORD CPlugins::FindFont( DWORD Encoding, BYTE Index )
{
	if ( Encoding == ENCODING_ASCII )
	{
		return FONTID_PC437;
	}

	DWORD RFontID = EncodingFontMap[ Encoding ][ EncodingFontSelectors[ Index ][ Encoding ] ];
	
	return RFontID;
}

std::vector<DWORD> CPlugins::FontListForEncoding( DWORD Encoding )
{
	std::vector<DWORD> RFontList;

	if ( EncodingFontMap.find( Encoding ) != EncodingFontMap.end() )
	{
		RFontList = EncodingFontMap[ Encoding ];
	}

	return RFontList;
}

NUTSFontNames CPlugins::FullFontList( void )
{
	return FontNames;
}

void CPlugins::NextFont( DWORD Encoding, BYTE Index )
{
	EncodingFontSelectors[ Index ][ Encoding ] =
		( EncodingFontSelectors[ Index ][ Encoding ] + 1 ) %
		EncodingFontMap[ Encoding ].size();
}

std::vector<FSMenu> CPlugins::GetFSMenu()
{
	std::vector<FSMenu> menus;

	DWORD CProviderID = 0xFFFF;

	FSDescriptor_iter fi;
	FSMenu            menu;

	fi = FSDescriptors.begin();

	while ( fi != FSDescriptors.end() )
	{
		DWORD ThisProvID = MAKEPROVID( PLUGINID( fi->PUID ), PROVIDERID( fi->PUID ) );

		if ( ThisProvID != CProviderID )
		{
			if ( CProviderID != 0xFFFF )
			{
				menus.push_back( menu );
			}

			menu.FS.clear();

			for ( NUTSProvider_iter pi = Providers.begin(); pi != Providers.end(); pi++ )
			{
				if ( PROVID( pi->ProviderID ) == PROVID( fi->PUID ) )
				{
					menu.Provider = pi->FriendlyName;
				}
			}

			CProviderID = ThisProvID;
		}

		FormatMenu format;

		format.FS = fi->FriendlyName;
		format.ID = fi->PUID;

		menu.FS.push_back( format );

		fi++;
	}

	menus.push_back( menu );

	/* Add ZIP Files manually */
	FormatMenu format;

	menu.Provider = L"PKWare";
	menu.FS.clear();

	format.FS     = L"ZIP File";
	format.ID     = FSID_ZIP;

	menu.FS.push_back( format );
	menus.push_back( menu );

	return menus;
}

void *CPlugins::LoadTranslator( DWORD TUID )
{
	void *pXlator = nullptr;
	
	NUTSPlugin *plugin = GetPlugin( TUID );

	if ( plugin != nullptr )
	{
		PluginCommand cmd;

		cmd.CommandID = PC_CreateTranslator;
		cmd.InParams[ 0 ].Value = TUID;

		if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			pXlator = cmd.OutParams[ 0 ].pPtr;
		}
	}

	return pXlator;
}

RootHookList CPlugins::GetRootHooks()
{
	return RootHooks;
}

RootCommandSet CPlugins::GetRootCommands()
{
	return RootCommands;
}

int CPlugins::PerformRootCommand( HWND hWnd, DWORD PUID, DWORD CmdIndex )
{
	NUTSPlugin *plugin = GetPlugin( MAKEFSID( PUID, 0, 0 ) );

	PluginCommand cmd;

	cmd.CommandID = PC_PerformRootCommand;
	cmd.InParams[ 0 ].pPtr = (void *) hWnd;
	cmd.InParams[ 1 ].Value = CmdIndex;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		return (int) cmd.OutParams[ 0 ].Value;
	}

	return NUTSError( 0x70, L"Declared command not recognised - this is a software bug." );
}

bool CPlugins::TranslateZIPContent( NativeFile *pFile, BYTE *pExtra )
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginCommand cmd;

		cmd.CommandID = PC_TranslateZIPContent;
		cmd.InParams[ 0 ].pPtr = pFile;
		cmd.InParams[ 1 ].pPtr = pExtra;

		if ( iter->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			if ( cmd.OutParams[ 0 ].Value != 0 )
			{
				return true;
			}
		}

		iter++;
	}

	return false;
}

void CPlugins::LoadImageExtensions( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_ReportImageExtensions;

	if ( plugin->CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
	{
		return;
	}

	BYTE NumExts = (BYTE) cmd.OutParams[ 0 ].Value;

	for ( BYTE Ext = 0; Ext < NumExts; Ext++ )
	{
		cmd.CommandID = PC_GetImageExtension;
		cmd.InParams[ 0 ].Value = Ext;

		if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			ExtReg.RegisterExtension(
				std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr ),
				(FileType) cmd.OutParams[ 1 ].Value,
				(FileType) cmd.OutParams[ 2 ].Value
			);
		}
	}
}

void CPlugins::LoadFonts( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_ReportEncodingCount;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		DWORD EncodingCount = cmd.OutParams[ 0 ].Value;

		cmd.CommandID = PC_SetEncodingBase;
		cmd.InParams[ 0 ].Value = EncodingID;

		if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			cmd.CommandID = PC_ReportFonts;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				DWORD NumFonts = cmd.OutParams[ 0 ].Value;

				for ( DWORD i = 0; i < NumFonts; i++ )
				{
					cmd.CommandID = PC_GetFontPointer;
					cmd.InParams[ 0 ].Value = i;
					cmd.InParams[ 1 ].Value = FontID;

					if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
					{
						BYTE *pFont      = (BYTE *) cmd.OutParams[ 0 ].pPtr;
						WCHAR *pFontName = (WCHAR *) cmd.OutParams[ 1 ].pPtr;

						FontList[ FontID ]  = pFont;
						FontNames[ FontID ] = pFontName;

						for ( BYTE e=2; e<8; e++ )
						{
							DWORD Encoding = cmd.OutParams[ e ].Value;

							if ( Encoding != 0U )
							{
								if ( EncodingFontMap.find( Encoding ) == EncodingFontMap.end() )
								{
									EncodingFontMap[ Encoding ] = std::vector<DWORD>();
								}

								EncodingFontMap[ Encoding ].push_back( FontID );

								EncodingFontSelectors[ 0 ][ Encoding ] = 0;
								EncodingFontSelectors[ 1 ][ Encoding ] = 0;

							}
							else
							{
								break;
							}
						}

						FontMap[ FontID ] = PluginID;

						FontID++;
					}
				}
			}

			EncodingID += EncodingCount;
		}
	}
}

void CPlugins::LoadIcons( NUTSPlugin *plugin )
{
	BYTE Icons;

	PluginCommand cmd;

	cmd.CommandID = PC_ReportIconCount;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		Icons = (BYTE) cmd.OutParams[ 0 ].Value;
		
		cmd.CommandID = PC_DescribeIcon;

		for ( BYTE Icon = 0; Icon < Icons; Icon++ )
		{
			cmd.InParams[ 0 ].Value = Icon;
			cmd.InParams[ 1 ].Value = IconID;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				HBITMAP hBitmap = (HBITMAP) cmd.OutParams[ 0 ].pPtr;

				BitmapCache.AddBitmap( IconID, hBitmap );

				IconID++;
			}
		}
	}
}

void CPlugins::LoadTranslators( NUTSPlugin *plugin )
{
	BYTE TX;

	PluginCommand cmd;

	cmd.CommandID = PC_ReportTranslators;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		BYTE NumTranslators = (BYTE) cmd.OutParams[ 0 ].Value;

		for ( TX = 0; TX<NumTranslators; TX++ )
		{
			cmd.CommandID = PC_DescribeTranslator;
			cmd.InParams[ 0 ].Value = TX;
			cmd.InParams[ 1 ].Value = TXID;
			cmd.InParams[ 2 ].Value = plugin->PluginID;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				DataTranslator DT;

				DT.FriendlyName = std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr);
				DT.Flags        = cmd.OutParams[ 1 ].Value;
				DT.TUID         = TXID;
				DT.ProviderID   = MAKEFSID( plugin->PluginID, cmd.OutParams[ 2 ].Value, TXID );

				TXID++;

				Translators.push_back( DT );
			}
		}
	}
}

void CPlugins::LoadRootHooks( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_ReportRootHooks;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		BYTE RHs = cmd.OutParams[ 0 ].Value;

		for ( BYTE RH=0; RH<RHs; RH++ )
		{
			cmd.InParams[ 0 ].Value = RH;
			cmd.InParams[ 1 ].Value = PluginID;

			cmd.CommandID = PC_DescribeRootHook;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				RootHooks.push_back( * (RootHook *) cmd.OutParams[ 0 ].pPtr );
			}
		}
	}
}

void CPlugins::LoadRootCommands( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_ReportRootCommands;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		BYTE RCs = cmd.OutParams[ 0 ].Value;

		for ( BYTE RC=0; RC<RCs; RC++ )
		{
			cmd.InParams[ 0 ].Value = RC;

			cmd.CommandID = PC_DescribeRootCommand;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				RootCommand c;

				c.CmdIndex = RC;
				c.PUID = PluginID;
				c.Text = std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr );

				RootCommands.push_back( c );
			}
		}
	}
}

std::wstring CPlugins::GetCharacterDescription( DWORD FontID, BYTE Char )
{
	std::wstring desc = L"";

	if ( ( FontID != FONTID_PC437 ) &&  ( FontMap.find( FontID ) != FontMap.end() ) )
	{
		NUTSPlugin *plugin = GetPlugin( MAKEFSID( FontMap[ FontID ], 0, 0 ) );

		PluginCommand cmd;

		cmd.CommandID = PC_DescribeChar;
		cmd.InParams[ 0 ].Value = FontID;
		cmd.InParams[ 1 ].Value = Char;

		if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
		{
			desc = (WCHAR *) cmd.OutParams[ 0 ].pPtr;
		}
	}

	if ( desc == L"" )
	{
		WCHAR x[2] = { (WCHAR) Char, 0 };

		if ( ( Char >= 0 ) && ( Char < 0x20 ) )
		{
			desc = L"Control code " + std::to_wstring( (long double) Char );
		}
		else if ( ( Char >= 0x20 ) && ( Char <= 0x7E ) )
		{
			desc = L"ASCII Character '" + std::wstring( x ) + L"'";
		}
		else
		{
			desc = L"Extended Character " + std::to_wstring( (long double) Char );
		}
	}

	return desc;
}

void CPlugins::UnloadPlugins()
{
	PluginList::iterator iPlugin;

	for ( iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		FreeLibrary( iPlugin->Handle );
	}

	Providers.clear();
	Providers.shrink_to_fit();
	FSDescriptors.clear();
	FSDescriptors.shrink_to_fit();
	FontList.clear();
	FontNames.clear();
	FontMap.clear();
	ImageOffsets.clear();
	Plugins.clear();
	Plugins.shrink_to_fit();
	Translators.clear();
	Translators.shrink_to_fit();
	RootHooks.clear();
	RootHooks.shrink_to_fit();
	RootCommands.clear();
	RootCommands.shrink_to_fit();
	EncodingFontMap.clear();
	EncodingFontSelectors[0].clear();
	EncodingFontSelectors[1].clear();

	free( pPC437Font );

	pPC437Font = nullptr;
}