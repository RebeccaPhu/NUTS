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
#include "PortManager.h"

#include "BuiltIns.h"

#include <string>
#include <algorithm>

#include "NUTSAPI.h"

extern DataSourceCollector *pCollector;
extern HWND hSplashWnd;

CPlugins FSPlugins;

static bool _ProcessFOPData( FOPData *pFOP )
{
	return FSPlugins.ProcessFOP( pFOP );
}

static void *_LoadFOPFS( FSIdentifier FSID, void *source )
{
	return (void *) FSPlugins.LoadFS( FSID, (DataSource *) source );
}

CPlugins::CPlugins()
{
	Plugins.clear();

	pPC437Font = nullptr;

	FontNames[ FONTID_PC437 ] = L"PC437";

	PluginSplash = L"";

	InitializeCriticalSection( &SplashLock );
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

	DeleteCriticalSection( &SplashLock );
}

void CPlugins::LoadPlugins()
{
	EncodingFontMap[ ENCODING_ASCII ] = std::vector<FontIdentifier>();
	EncodingFontMap[ ENCODING_ASCII ].push_back( FONTID_PC437 );

	EncodingFontSelectors[ 0 ][ ENCODING_ASCII ] = 0;
	EncodingFontSelectors[ 1 ][ ENCODING_ASCII ] = 0;

	Providers = NUTSBuiltIns.GetBuiltInProviders();

	IconID = 0x1000;

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

	PortConfig.ReadConfiguration();

	SetPortConfiguration();

	SetSplash( L"Loaded " + std::to_wstring( (QWORD) Providers.size() ) + L" providers." );

	::PostMessage( hSplashWnd, WM_PAINT, 0, 0 );
	::PostMessage( hSplashWnd, WM_ENDSPLASH, 0, 0 );
}

void CPlugins::SetPortConfiguration( void )
{
	std::vector<ConfiguredDevicePort> cports = PortConfig.GetConfiguration();

	for ( NUTSProvider_iter iProvider = Providers.begin(); iProvider != Providers.end(); iProvider++ )
	{
		PluginCommand PC;

		PC.CommandID = PC_SetPortAssignments;

		for ( int i = 0; i < 8; i++ ) { PC.InParams[ i ].Value = 0; }

		int j = 0;

		for ( std::vector<ConfiguredDevicePort>::iterator iCPort = cports.begin(); iCPort != cports.end(); iCPort++ )
		{
			if ( j == 8 ) { break; }

			if ( iCPort->PLID == iProvider->ProviderID )
			{
				DWORD PortID = (DWORD ) iCPort->Index;

				if ( iCPort->Type == PortTypeParallel )
				{
					PortID |= 0x10000;
				}

				PC.InParams[ j ].Value = PortID;

				j++;
			}
		}

		if ( j > 0 )
		{
			NUTSPlugin *plugin = GetPluginByID( iProvider->PluginID );

			if ( plugin != nullptr )
			{
				(void) plugin->CommandHandler( &PC );
			}
		}
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
		plugin.CommandHandler = (fnNUTSPluginFunction)   GetProcAddress( hModule, "NUTSCommandHandler" );

		if ( plugin.CommandHandler == nullptr )
		{
			FreeLibrary( hModule );

			return;
		}

		PluginCommand cmd;

		/* Initialise the plugin and set the connector points */
		cmd.CommandID = PC_SetPluginConnectors;
		cmd.InParams[ 0 ].pPtr  = (void *) pCollector;
		cmd.InParams[ 1 ].pPtr  = (void *) pGlobalError;
		cmd.InParams[ 2 ].Value = NUTS_PREFERRED_API_VERSION;

		if ( plugin.CommandHandler( &cmd ) != NUTS_PLUGIN_SUCCESS )
		{
			FreeLibrary( hModule );

			return;
		}

		plugin.PluginID = PluginIdentifier( (WCHAR *) cmd.OutParams[ 1 ].pPtr );
		plugin.MaxAPI   = min( NUTS_PREFERRED_API_VERSION, cmd.OutParams[ 0 ].Value );

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
			
			Providers.push_back( p );

			SetSplash( L"Loaded Plugin: " + p.FriendlyName );

			::PostMessage( hSplashWnd, WM_PAINT, 0, 0 );

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
				GetFileSystemDetails( &plugin, pid, fsid, p.ProviderID, p.PluginID );
			}

		}

		LoadFonts( &plugin );
		LoadIcons( &plugin );
		LoadImageExtensions( &plugin );
		LoadTranslators( &plugin );
		LoadRootHooks( &plugin );
		LoadRootCommands( &plugin );
		LoadFOPDirectoryTypes( &plugin );
		LoadWrappers( &plugin );
	}
}

void CPlugins::LoadFOPDirectoryTypes( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_GetFOPDirectoryTypes;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		for ( int i=0; i<8; i++ )
		{
			if ( cmd.OutParams[ i ].pPtr == nullptr )
			{
				break;
			}

			FOPDirectoryType *pFT = (FOPDirectoryType *) cmd.OutParams[ i ].pPtr;

			DirectoryTypes[ pFT->Identifier ] = pFT->FriendlyName;
		}
	}
}

void CPlugins::LoadWrappers( NUTSPlugin *plugin )
{
	PluginCommand cmd;

	cmd.CommandID = PC_GetWrapperCount;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		int wrappers = (int) cmd.OutParams[ 0 ].Value;

		for ( int i=0; i<wrappers; i++ )
		{
			cmd.CommandID = PC_GetWrapperDescriptor;
			cmd.InParams[ 0 ].Value = (DWORD) i;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				WrapperDescriptor *pDesc = (WrapperDescriptor *) cmd.OutParams[ 0 ].pPtr;

				Wrappers.push_back( * pDesc );
				
				WrapperPluginMap[ pDesc->Identifier ] = plugin->PluginID;
			}
		}
	}
}

void CPlugins::GetFileSystemDetails( NUTSPlugin *plugin, BYTE pid, BYTE fsid, ProviderIdentifier PVID, PluginIdentifier PLID )
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

	FSDescriptors.push_back( fs );

	if ( ProviderFS.find( PVID ) == ProviderFS.end() )
	{
		ProviderFS[ PVID ] = std::vector<FSIdentifier>();
	}

	ProviderFS[ PVID ].push_back( fs.FSID );

	if ( PluginFS.find( PLID ) == PluginFS.end() )
	{
		PluginFS[ PLID ] = std::vector<FSIdentifier>();
	}

	PluginFS[ PLID ].push_back( fs.FSID );
}

NUTSProviderList CPlugins::GetProviders( void )
{
	return Providers;
}

FSDescriptorList CPlugins::GetFilesystems( ProviderIdentifier ProviderID )
{
	FSDescriptorList l;

	FSDescriptor_iter i;

	std::vector<FSIdentifier>::iterator iProviderFS;

	for ( iProviderFS = ProviderFS[ ProviderID ].begin(); iProviderFS != ProviderFS[ ProviderID ].end(); iProviderFS++ )
	{
		for ( i = FSDescriptors.begin(); i != FSDescriptors.end(); i++ )
		{
			if ( *iProviderFS == i->FSID )
			{
				l.push_back( *i );
			}
		}
	}

	return l;
}

FormatList CPlugins::GetFormats( ProviderIdentifier ProviderID, bool ExcludeUnimageable )
{
	FormatList Formats;

	Formats = NUTSBuiltIns.GetBuiltinFormatList( ProviderID, ExcludeUnimageable );

	if ( Formats.size() > 0 )
	{
		return Formats;
	}

	FSDescriptor_iter iFS;

	std::vector<FSIdentifier>::iterator iProviderFS;

	for ( iProviderFS = ProviderFS[ ProviderID ].begin(); iProviderFS != ProviderFS[ ProviderID ].end(); iProviderFS++ )
	{
		for ( iFS = FSDescriptors.begin(); iFS != FSDescriptors.end(); iFS++ )
		{
			if ( iFS->FSID == *iProviderFS )
			{
				FormatDesc Format;

				Format.Format     = iFS->FriendlyName;
				Format.FSID       = iFS->FSID;
				Format.Flags      = iFS->Flags;
				Format.MaxSize    = iFS->MaxSize;
				Format.SectorSize = iFS->SectorSize;

				Format.PreferredExtension = iFS->FavouredExtension;

				if ( ExcludeUnimageable )
				{
					if ( iFS->Flags & FSF_Imaging )
					{
						Formats.push_back( Format );
					}
				}
				else
				{
					Formats.push_back( Format );
				}
			}
		}
	}

	return Formats;
}

FSDescriptor CPlugins::GetFSDesc( FSIdentifier FSID )
{
	FSDescriptor fs;

	fs.FSID  = FSID_NONE;
	fs.Flags = 0;

	FSDescriptor_iter iFS;

	for ( iFS = FSDescriptors.begin(); iFS != FSDescriptors.end(); iFS++ )
	{
		if ( iFS->FSID == FSID )
		{
			return *iFS;
		}
	}

	return fs;
}

TranslatorList CPlugins::GetTranslators( ProviderIdentifier PVID, DWORD Type )
{
	TranslatorList Xlators;

	TranslatorIterator iter;

	for ( iter = Translators.begin(); iter != Translators.end(); iter++ )
	{
		if ( iter->Flags & Type )
		{
			if ( ( iter->ProviderID == PVID ) || ( PVID == Provider_Null ) )
			{
				Xlators.push_back( *iter );
			}
		}
	}

	return Xlators;
}

FOPDirectoryTypes CPlugins::GetDirectoryTypes()
{
	return DirectoryTypes;
}

FSHints CPlugins::FindFS( DataSource *pSource, NativeFile *pFile )
{
	std::vector<FSHint> hints = NUTSBuiltIns.GetOffers( pSource, pFile );
	
	FSDescriptor_iter iter = FSDescriptors.begin();

	while ( iter != FSDescriptors.end() )
	{
		FSHint hint  = { FS_Null, 0 };

		FileSystem *pFS = LoadFS( iter->FSID, pSource );

		if ( pFS == nullptr )
		{
			iter++;

			continue;
		}

		pFS->FSID = iter->FSID;

		if ( pFile == nullptr )
		{
			hint = pFS->Offer( nullptr );
		}
		else
		{
			hint = pFS->Offer( pFile->Extension );
		}

		hint.FSID = iter->FSID;

		if ( hint.Confidence > 0 )
		{
			hints.push_back( hint );
		}

		iter++;

		delete pFS;

		pCollector->ReleaseSources();
	}

	return hints;
}

FileSystem *CPlugins::FindAndLoadFS( DataSource *pSource, NativeFile *pFile )
{
	WrapperList wrappers = GetWrappers();

	FSHints hints = FindFS( pSource, pFile );

	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		iHint->WrapperID = WID_Null;
	}

	for ( WrapperList::iterator iWrapper = wrappers.begin(); iWrapper != wrappers.end(); iWrapper++ )
	{
		DataSource *pWrappedSource = LoadWrapper( iWrapper->Identifier, pSource );

		if ( !pWrappedSource->Valid() )
		{
			DS_RELEASE( pWrappedSource );

			continue;
		}

		FSHints WrapperHints = FindFS( pWrappedSource, pFile );

		for ( FSHints::iterator iHint = WrapperHints.begin(); iHint != WrapperHints.end(); iHint++ )
		{
			FSHint hint = *iHint;

			hint.WrapperID = iWrapper->Identifier;

			hints.push_back( hint );
		}

		DS_RELEASE( pWrappedSource );
	}

	WORD Confidence = 0;

	FSIdentifier FSID           = FS_Null;
	WrapperIdentifier WrapperID = WID_Null;
	
	FileSystem *newFS = nullptr;

	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		if ( iHint->Confidence > Confidence )
		{
			Confidence = iHint->Confidence;
			FSID       = iHint->FSID;
			WrapperID  = iHint->WrapperID;
		}
	}

	if ( FSID != FS_Null )
	{
		if ( WrapperID != WID_Null )
		{
			DataSource *pWrappedSource = LoadWrapper( WrapperID, pSource );

			if ( pWrappedSource != nullptr )
			{
				newFS = LoadFS( FSID, pWrappedSource );
			}

			DS_RELEASE( pWrappedSource );
		}
		else
		{
			newFS = LoadFS( FSID, pSource );
		}
	}

	return newFS;
}

FileSystem *CPlugins::LoadFSWithWrappers( FSIdentifier FSID, DataSource *pSource )
{
	FileSystem *newFS = nullptr;

	WrapperList wrappers = GetWrappers();

	FileSystem *pLoadFS = LoadFS( FSID, pSource );

	if ( pLoadFS == nullptr )
	{
		return nullptr;
	}

	FSHints hints;
	
	hints.push_back( pLoadFS->Offer( nullptr ) );

	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		iHint->WrapperID = WID_Null;
	}

	delete pLoadFS;

	for ( WrapperList::iterator iWrapper = wrappers.begin(); iWrapper != wrappers.end(); iWrapper++ )
	{
		DataSource *pWrappedSource = LoadWrapper( iWrapper->Identifier, pSource );

		if ( !pWrappedSource->Valid() )
		{
			DS_RELEASE( pWrappedSource );

			continue;
		}

		pLoadFS = LoadFS( FSID, pWrappedSource );

		FSHint hint = pLoadFS->Offer( nullptr );

		hint.WrapperID = iWrapper->Identifier;

		hints.push_back( hint );

		DS_RELEASE( pWrappedSource );
	}

	WORD Confidence = 0;

	FSIdentifier FSID           = FS_Null;
	WrapperIdentifier WrapperID = WID_Null;
	
	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		if ( iHint->Confidence > Confidence )
		{
			Confidence = iHint->Confidence;
			FSID       = iHint->FSID;
			WrapperID  = iHint->WrapperID;
		}
	}

	if ( FSID != FS_Null )
	{
		if ( WrapperID != WID_Null )
		{
			DataSource *pWrappedSource = LoadWrapper( WrapperID, pSource );

			if ( pWrappedSource != nullptr )
			{
				newFS = LoadFS( FSID, pWrappedSource );
			}

			DS_RELEASE( pWrappedSource );
		}
		else
		{
			newFS = LoadFS( FSID, pSource );
		}
	}

	return newFS;
}

FileSystem *CPlugins::LoadFS( FSIdentifier FSID, DataSource *pSource )
{
	FileSystem *pBuiltIn = NUTSBuiltIns.LoadFS( FSID, pSource );

	if ( pBuiltIn != nullptr )
	{
		pBuiltIn->ProcessFOP = _ProcessFOPData;
		pBuiltIn->LoadFOPFS  = _LoadFOPFS;

		if ( pSource->Flags & DS_ReadOnly )
		{
			pBuiltIn->Flags |= FSF_ReadOnly;
		}

		return pBuiltIn;
	}
	
	FSDescriptor_iter iter = FSDescriptors.begin();

	DataSource *pAuxSource   = nullptr;
	DataSource *pCloneSource = pSource;

	while ( iter != FSDescriptors.end() )
	{
		if ( iter->FSID == FSID )
		{
			FileSystem *pFS = nullptr;

			NUTSPlugin *plugin = GetPlugin( FSID );

			if ( plugin != nullptr )
			{
				PluginCommand cmd;

				cmd.CommandID = PC_CreateFileSystem;
				cmd.InParams[ 0 ].pPtr = (void * ) FSID.c_str();
				cmd.InParams[ 2 ].pPtr = (void *) pCloneSource;
				
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

			pFS->ProcessFOP = _ProcessFOPData;
			pFS->LoadFOPFS  = _LoadFOPFS;

			if ( pSource->Flags & DS_ReadOnly )
			{
				pFS->Flags |= FSF_ReadOnly;
			}

			return pFS;
		}

		for ( RootHookIterator i = RootHooks.begin(); i != RootHooks.end(); i++ )
		{
			if ( i->HookFSID == FSID )
			{
				NUTSPlugin *p = GetPlugin( i->HookFSID );

				PluginCommand cmd;

				cmd.CommandID = PC_CreateFileSystem;
				cmd.InParams[ 0 ].pPtr = (void *) FSID.c_str();
				cmd.InParams[ 2 ].pPtr = (void *) pSource;

				if ( p->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
				{
					FileSystem *pFS = (FileSystem *) cmd.OutParams[ 0 ].pPtr;

					if ( pFS != nullptr )
					{
						pFS->ProcessFOP = _ProcessFOPData;

						if ( pSource->Flags & DS_ReadOnly )
						{
							pFS->Flags |= FSF_ReadOnly;
						}
					}

					return pFS;
				}
			}
		}

		iter++;
	}

	return nullptr;
}

NUTSPlugin *CPlugins::GetPlugin( FSIdentifier FSID )
{
	PluginIdentifier PLID = L"";

	PluginFSMap::iterator iPL;

	for ( iPL = PluginFS.begin(); iPL != PluginFS.end(); iPL++ )
	{
		for ( std::vector<FSIdentifier>::iterator iFS = iPL->second.begin(); iFS != iPL->second.end(); iFS++ )
		{
			if ( *iFS == FSID )
			{
				PLID = iPL->first;
			}
		}
	}

	for ( Plugin_iter i = Plugins.begin(); i != Plugins.end(); i++ )
	{
		if ( i->PluginID == PLID )
		{
			return &*i;
		}
	}

	return nullptr;
}

NUTSPlugin *CPlugins::GetTXPlugin( TXIdentifier TXID )
{
	PluginIdentifier PLID = L"";

	TranslatorIterator iTX;

	for ( iTX = Translators.begin(); iTX != Translators.end(); iTX++ )
	{
		if ( iTX->TUID == TXID )
		{
			NUTSProvider_iter iPV;

			for ( iPV = Providers.begin(); iPV != Providers.end(); iPV++ )
			{
				if ( iPV->ProviderID == iTX->ProviderID )
				{
					PLID = iPV->PluginID;
				}
			}
		}
	}

	for ( Plugin_iter i = Plugins.begin(); i != Plugins.end(); i++ )
	{
		if ( i->PluginID == PLID )
		{
			return &*i;
		}
	}

	return nullptr;
}

NUTSPlugin *CPlugins::GetPluginByID( PluginIdentifier PLID )
{
	for ( Plugin_iter i = Plugins.begin(); i != Plugins.end(); i++ )
	{
		if ( i->PluginID == PLID )
		{
			return &*i;
		}
	}

	return nullptr;
}

std::wstring CPlugins::ProviderName( ProviderIdentifier PRID )
{
	std::wstring BuiltInProviderName = NUTSBuiltIns.ProviderName( PRID );

	if ( BuiltInProviderName != L"" )
	{
		return BuiltInProviderName;
	}

	std::wstring name = L"Unknown Provider";

	FSDescriptor_iter fi;

	for ( NUTSProvider_iter iProvider = Providers.begin(); iProvider != Providers.end(); iProvider++ )
	{
		if ( iProvider->ProviderID == PRID )
		{
			name = iProvider->FriendlyName;

			return name;
		}
	}

	return name;
}

std::wstring CPlugins::FSName( FSIdentifier FSID )
{
	std::wstring BuiltInFSName = NUTSBuiltIns.FSName( FSID );

	if ( BuiltInFSName != L"" )
	{
		return BuiltInFSName;
	}

	std::wstring name = L"Unknown File System";

	FSDescriptor_iter fi;

	for ( fi = FSDescriptors.begin(); fi != FSDescriptors.end(); fi++ )
	{
		if ( fi->FSID == FSID )
		{
			name = fi->FriendlyName;

			return name;
		}
	}

	return name;
}

std::wstring CPlugins::FSExt( FSIdentifier FSID )
{
	std::wstring BuiltInFSExt = NUTSBuiltIns.FSExt( FSID );

	if ( BuiltInFSExt != L"" )
	{
		return BuiltInFSExt;
	}

	std::wstring ext = L"Img";

	FSDescriptor_iter fi;

	for ( fi = FSDescriptors.begin(); fi != FSDescriptors.end(); fi++ )
	{
		if ( fi->FSID == FSID )
		{
			ext = UString( fi->FavouredExtension );

			return ext;
		}
	}

	return ext;
}

std::wstring CPlugins::FontName( FontIdentifier ReqFontID )
{
	static WCHAR *PC437Name = L"PC437";

	if ( ReqFontID == FONTID_PC437 )
	{
		return FontIdentifier( PC437Name );
	}

	if ( FontNames.find( ReqFontID ) != FontNames.end() )
	{
		return FontNames[ ReqFontID ];
	}

	return PC437Name;
}

void *CPlugins::LoadFont( FontIdentifier ReqFontID )
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

FontIdentifier CPlugins::FindFont( EncodingIdentifier Encoding, BYTE Index )
{
	if ( Encoding == ENCODING_ASCII )
	{
		return FONTID_PC437;
	}

	FontIdentifier RFontID = EncodingFontMap[ Encoding ][ EncodingFontSelectors[ Index ][ Encoding ] ];
	
	return RFontID;
}

std::vector<FontIdentifier> CPlugins::FontListForEncoding( EncodingIdentifier Encoding )
{
	std::vector<FontIdentifier> RFontList;

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

void CPlugins::NextFont( EncodingIdentifier Encoding, BYTE Index )
{
	EncodingFontSelectors[ Index ][ Encoding ] =
		( EncodingFontSelectors[ Index ][ Encoding ] + 1 ) %
		EncodingFontMap[ Encoding ].size();
}

static bool FSMenuSort( FSMenu &a, FSMenu &b )
{
	/*
	if ( wcscmp( a.Provider.c_str(), b.Provider.c_str() ) < 0 )
	{
		return true;
	}
	*/

	if ( a.Provider < b.Provider )
	{
		return true;
	}
	
	return false;
}

std::vector<FSMenu> CPlugins::GetFSMenu()
{
	std::vector<FSMenu> menus = NUTSBuiltIns.GetBuiltInMenuList();

	ProviderIdentifier CProviderID = L"No_Provider";

	FSDescriptor_iter fi;
	FSMenu            menu;

	fi = FSDescriptors.begin();

	while ( fi != FSDescriptors.end() )
	{
		ProviderIdentifier PVID = L"Unset_Provider";

		ProviderFSMap::iterator iPFS;

		for ( iPFS = ProviderFS.begin(); iPFS != ProviderFS.end(); iPFS++ )
		{
			for ( std::vector<FSIdentifier>::iterator iFS = iPFS->second.begin(); iFS != iPFS->second.end(); iFS++ )
			{
				if ( fi->FSID == *iFS )
				{
					PVID = iPFS->first;
				}
			}
		}

		if ( PVID != CProviderID )
		{
			if ( CProviderID != L"No_Provider" )
			{
				menus.push_back( menu );
			}

			menu.FS.clear();

			for ( NUTSProvider_iter pi = Providers.begin(); pi != Providers.end(); pi++ )
			{
				if ( pi->ProviderID == PVID )
				{
					menu.Provider = pi->FriendlyName;
				}
			}

			CProviderID = PVID;
		}

		FormatMenu format;

		format.FS = fi->FriendlyName;
		format.ID = fi->FSID;

		menu.FS.push_back( format );

		fi++;
	}

	menus.push_back( menu );

	std::sort( menus.begin(), menus.end(), FSMenuSort );

	return menus;
}

void *CPlugins::LoadTranslator( TXIdentifier TUID )
{
	void *pXlator = nullptr;
	
	NUTSPlugin *plugin = GetTXPlugin( TUID );

	if ( plugin != nullptr )
	{
		PluginCommand cmd;

		cmd.CommandID = PC_CreateTranslator;
		cmd.InParams[ 0 ].pPtr = (void *) TUID.c_str();

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

int CPlugins::PerformRootCommand( HWND hWnd, PluginIdentifier PUID, DWORD CmdIndex )
{
	NUTSPlugin *plugin = GetPlugin( PUID );

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

bool CPlugins::ProcessFOP( FOPData *_FOPData )
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginCommand cmd;

		cmd.CommandID = PC_TranslateFOPContent;
		cmd.InParams[ 0 ].pPtr = _FOPData;

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

	cmd.CommandID = PC_ReportFonts;

	if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		DWORD NumFonts = cmd.OutParams[ 0 ].Value;

		for ( DWORD i = 0; i < NumFonts; i++ )
		{
			cmd.CommandID = PC_GetFontPointer;
			cmd.InParams[ 0 ].Value = i;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				BYTE *pFont             = (BYTE *)  cmd.OutParams[ 0 ].pPtr;
				std::wstring   FontName = (WCHAR *) cmd.OutParams[ 1 ].pPtr;
				FontIdentifier FontID   = (WCHAR *) cmd.OutParams[ 2 ].pPtr;

				FontList[ FontID ]  = pFont;
				FontNames[ FontID ] = FontName;

				for ( BYTE e=3; e<8; e++ )
				{
					if ( cmd.OutParams[ e ].pPtr != nullptr )
					{
						EncodingIdentifier Encoding = EncodingIdentifier( (WCHAR *) cmd.OutParams[ e ].pPtr );

						if ( EncodingFontMap.find( Encoding ) == EncodingFontMap.end() )
						{
							EncodingFontMap[ Encoding ] = std::vector<FontIdentifer>();
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

				FontMap[ FontID ] = plugin->PluginID;
			}
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

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				DataTranslator DT;

				DT.FriendlyName = std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr);
				DT.Flags        = cmd.OutParams[ 1 ].Value;
				DT.TUID         = std::wstring( (WCHAR *) cmd.OutParams[ 3 ].pPtr);
				DT.ProviderID   = std::wstring( (WCHAR *) cmd.OutParams[ 2 ].pPtr);

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
		BYTE RHs = (BYTE) cmd.OutParams[ 0 ].Value;

		for ( BYTE RH=0; RH<RHs; RH++ )
		{
			cmd.InParams[ 0 ].Value = RH;

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
		BYTE RCs = (BYTE ) cmd.OutParams[ 0 ].Value;

		for ( BYTE RC=0; RC<RCs; RC++ )
		{
			cmd.InParams[ 0 ].Value = RC;

			cmd.CommandID = PC_DescribeRootCommand;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				RootCommand c;

				c.CmdIndex = RC;
				c.PLID = plugin->PluginID;
				c.Text = std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr );

				RootCommands.push_back( c );
			}
		}
	}
}

std::wstring CPlugins::GetCharacterDescription( FontIdentifier FontID, BYTE Char )
{
	std::wstring desc = L"";

	if ( ( FontID != FONTID_PC437 ) &&  ( FontMap.find( FontID ) != FontMap.end() ) )
	{
		NUTSPlugin *plugin = GetPluginByID( FontMap[ FontID ] );

		if ( plugin != nullptr )
		{
			PluginCommand cmd;

			cmd.CommandID = PC_DescribeChar;
			cmd.InParams[ 0 ].pPtr  = (void *) FontID.c_str();
			cmd.InParams[ 1 ].Value = Char;

			if ( plugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				desc = (WCHAR *) cmd.OutParams[ 0 ].pPtr;
			}
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

std::wstring CPlugins::GetProviderNameByID( ProviderIdentifier prid )
{
	for ( NUTSProvider_iter iProv = Providers.begin(); iProv != Providers.end(); iProv++ )
	{
		if ( iProv->ProviderID == prid )
		{
			return iProv->FriendlyName;
		}
	}

	return L"";
}

std::vector<NUTSPortRequirement> CPlugins::GetPortRequirements( void )
{
	std::vector<NUTSPortRequirement> reqs;

	for ( Plugin_iter iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		PluginCommand PC;

		PC.CommandID = PC_GetPortProviders;

		if ( iPlugin->CommandHandler( &PC ) == NUTS_PLUGIN_SUCCESS )
		{
			NUTSPortRequirement req;

			for ( DWORD i=0; i<8; i++ )
			{
				if ( PC.OutParams[ i ].pPtr == nullptr )
				{
					break;
				}

				PluginCommand PC2;

				PC2.CommandID = PC_GetPortCounts;
				PC2.InParams[ 0 ].Value = i;

				if ( iPlugin->CommandHandler( &PC2 ) == NUTS_PLUGIN_SUCCESS )
				{
					req.ProviderID = std::wstring( (WCHAR *) PC.OutParams[ i ].pPtr );
					req.PortCount  = (BYTE ) PC2.OutParams[ 0 ].Value;

					reqs.push_back( req );
				}
			}
		}
	}

	return reqs;
}

WrapperList CPlugins::GetWrappers()
{
	WrapperList wrappers = NUTSBuiltIns.GetWrappers();

	for ( Wrapper_iter iWrapper = Wrappers.begin(); iWrapper != Wrappers.end(); iWrapper++ )
	{
		wrappers.push_back( *iWrapper );
	}

	return wrappers;
}

std::wstring CPlugins::GetWrapperName( WrapperIdentifier wrapper )
{
	std::wstring name = L"";

	WrapperList wrappers = GetWrappers();

	for ( Wrapper_iter iWrapper = wrappers.begin(); iWrapper != wrappers.end(); iWrapper++ )
	{
		if ( iWrapper->Identifier == wrapper )
		{
			name = iWrapper->FriendlyName;
		}
	}

	return name;
}

DataSource *CPlugins::LoadWrapper( WrapperIdentifier wrapper, DataSource *pSource )
{
	DataSource *pWrapper = NUTSBuiltIns.GetWrapper( wrapper, pSource );

	if ( pWrapper == nullptr )
	{
		if ( WrapperPluginMap.find( wrapper ) != WrapperPluginMap.end() )
		{
			PluginIdentifier plugin = WrapperPluginMap[ wrapper ];

			NUTSPlugin *pPlugin = GetPluginByID( plugin );

			PluginCommand cmd;

			cmd.CommandID = PC_LoadWrapper;
			cmd.InParams[ 0 ].pPtr = (void *) wrapper.c_str();
			cmd.InParams[ 1 ].pPtr = (void *) pSource;

			if ( pPlugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				pWrapper = (DataSource *) cmd.OutParams[ 0 ].pPtr;
			}
		}
	}

	return pWrapper;
}