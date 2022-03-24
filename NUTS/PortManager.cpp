#include "StdAfx.h"
#include "PortManager.h"

#include <WbemIdl.h>
#include <comutil.h>

#include "resource.h"
#include "NUTS.h"
#include "Preference.h"
#include "Plugins.h"
#include "libfuncs.h"

PortManager PortConfig;

PortManager::PortManager(void)
{
}

PortManager::~PortManager(void)
{
}

void PopulateList( HWND hDlg )
{
	HWND hList = GetDlgItem( hDlg, IDC_PORTLIST );

	std::vector<DevicePort> ports = PortConfig.EnumeratePorts();

	std::vector<ConfiguredDevicePort> Config = PortConfig.GetConfiguration();

	for ( std::vector<ConfiguredDevicePort>::iterator iCPort = Config.begin(); iCPort != Config.end(); iCPort++ )
	{
		for ( std::vector<DevicePort>::iterator iPort = ports.begin(); iPort != ports.end(); iPort++ )
		{
			if ( ( iPort->Index == iCPort->Index ) && ( iPort->Type == iCPort->Type ) )
			{
				LVITEM lvi;

				lvi.pszText  = (WCHAR *) iPort->Name.c_str();
				lvi.mask     = LVIF_TEXT | LVIF_IMAGE;
				lvi.iItem    = 0;
				lvi.iSubItem = 0;
				lvi.iImage   = 0;
			
				ListView_InsertItem( hList, &lvi );

				lvi.pszText  = (WCHAR *) iCPort->Name.c_str();
				lvi.iSubItem = 1;

				ListView_SetItem( hList, &lvi );
			}
		}
	}
}

std::vector<NUTSPortRequirement>  IP_reqs;
std::vector<DevicePort>           IP_ports;
std::vector<ConfiguredDevicePort> IP_Config;

bool           PortPicked;
DevicePortType PickedType;
BYTE           PickedIndex;

ProviderIdentifier PickedID;
std::wstring       PickedName;

INT_PTR CALLBACK InsertPortFunc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
	case WM_INITDIALOG:
		{
			PortPicked = false;

			IP_reqs   = FSPlugins.GetPortRequirements();
			IP_ports  = PortConfig.EnumeratePorts();
			IP_Config = PortConfig.GetConfiguration();

			// Use the configuration to pre-decrement port counts so we don't offer more ports than a plugin can handle.
			for ( std::vector<ConfiguredDevicePort>::iterator iCPort = IP_Config.begin(); iCPort != IP_Config.end(); iCPort++ )
			{
				for ( std::vector<NUTSPortRequirement>::iterator iReq = IP_reqs.begin(); iReq != IP_reqs.end(); iReq++ )
				{
					if ( iReq->ProviderID == iCPort->PLID )
					{
						iReq->PortCount--;
					}
				}

				for ( std::vector<DevicePort>::iterator iPort = IP_ports.begin(); iPort != IP_ports.end(); )
				{
					if ( ( iPort->Index == iCPort->Index ) && ( iPort->Type == iCPort->Type ) )
					{
						iPort = IP_ports.erase( iPort );
					}
					else
					{
						iPort++;
					}
				}
			}

			for ( std::vector<DevicePort>::iterator iPort = IP_ports.begin(); iPort != IP_ports.end(); iPort++ )
			{
				::SendMessage( GetDlgItem( hDlg, IDC_PORT_SELECT ), CB_ADDSTRING, 0, (LPARAM) iPort->Name.c_str() );
			}

			if ( IP_ports.size() > 0 )
			{
				::SendMessage( GetDlgItem( hDlg, IDC_PORT_SELECT ), CB_SETCURSEL, 0, 0 );
			}
			else
			{
				EnableWindow( GetDlgItem( hDlg, IDC_PORT_SELECT ), FALSE );
			}

			int NumPlugins = 0;

			for ( std::vector<NUTSPortRequirement>::iterator iReq = IP_reqs.begin(); iReq != IP_reqs.end(); iReq++ )
			{
				if ( iReq->PortCount == 0 )
				{
					continue;
				}

				::SendMessage( GetDlgItem( hDlg, IDC_PLUGIN_SELECT ), CB_ADDSTRING, 0, (LPARAM) FSPlugins.GetProviderNameByID( iReq->ProviderID ).c_str() );

				NumPlugins++;
			}

			if ( NumPlugins > 0 )
			{
				::SendMessage( GetDlgItem( hDlg, IDC_PLUGIN_SELECT ), CB_SETCURSEL, 0, 0 );
			}
			else
			{
				EnableWindow( GetDlgItem( hDlg, IDC_PLUGIN_SELECT ), FALSE );
			}
		}
		break;

	case WM_COMMAND:
		if ( LOWORD( wParam ) == IDOK )
		{
			if ( ( IP_ports.size() > 0 ) && ( IP_reqs.size() > 0 ) )
			{
				PortPicked = true;

				int PortID = ::SendMessage( GetDlgItem( hDlg, IDC_PORT_SELECT ), CB_GETCURSEL, 0, 0 );

				PickedIndex = IP_ports[ PortID ].Index;
				PickedType  = IP_ports[ PortID ].Type;
				PickedName  = IP_ports[ PortID ].Name;

				int ProvID = ::SendMessage( GetDlgItem( hDlg, IDC_PORT_SELECT ), CB_GETCURSEL, 0, 0 );

				PickedID    = IP_reqs[ ProvID ].ProviderID;
			}

			EndDialog( hDlg, IDOK );
		}
		else if ( LOWORD( wParam ) == IDCANCEL )
		{
			EndDialog( hDlg, IDCANCEL );
		}

		break;
	}

	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK PortConfigFunc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
	case WM_INITDIALOG:
		{
			HWND hList = GetDlgItem( hDlg, IDC_PORTLIST );

			HIMAGELIST hSmall = ImageList_Create( 16, 16, ILC_MASK, 1, 1 );

			HICON hPort =  LoadIcon( hInst, MAKEINTRESOURCE( IDI_PORT ) );

			ImageList_AddIcon( hSmall, hPort );

			DestroyIcon( hPort );

			ListView_SetImageList( hList, hSmall, LVSIL_SMALL );

			LVCOLUMN lvc;

			lvc.mask = LVCF_FMT | LVCF_WIDTH |LVCF_TEXT | LVCF_SUBITEM;
			lvc.iSubItem = 0;
			lvc.pszText  = L"Port";
			lvc.cx = 180;
			lvc.fmt = LVCFMT_LEFT;

			ListView_InsertColumn( hList, 0, &lvc );

			RECT r;

			GetClientRect( hList, &r );

			lvc.pszText = L"Provider";
			lvc.iSubItem = 1;
			lvc.cx = ( r.right - r.left ) - ( 180 + 8 ); // Sod the metrics.

			ListView_InsertColumn( hList, 1, &lvc );

			PopulateList( hDlg );
		}
		break;

	case WM_COMMAND:
		if ( LOWORD( wParam ) == IDOK )
		{
			EndDialog( hDlg, IDOK );
		}
		else if ( LOWORD( wParam ) == IDC_ADD_PORT )
		{
			if ( DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_INSERT_PORT ), hDlg, InsertPortFunc, NULL ) == IDOK )
			{
				if ( PortPicked )
				{
					ConfiguredDevicePort dp;

					dp.Index = PickedIndex;
					dp.Type  = PickedType;
					dp.PLID  = PickedID;
					dp.Name  = FSPlugins.GetProviderNameByID( PickedID );

					PortConfig.AddPort( dp );

					PortConfig.StoreConfiguration();

					HWND hList = GetDlgItem( hDlg, IDC_PORTLIST );

					LVITEM lvi;

					lvi.pszText  = (WCHAR *) PickedName.c_str();
					lvi.mask     = LVIF_TEXT | LVIF_IMAGE;
					lvi.iItem    = 0;
					lvi.iSubItem = 0;
					lvi.iImage   = 0;
			
					ListView_InsertItem( hList, &lvi );

					lvi.pszText  = (WCHAR *) dp.Name.c_str();
					lvi.iSubItem = 1;

					ListView_SetItem( hList, &lvi );
				}
			}
		}
		else if ( LOWORD( wParam ) == IDC_REMOVE_PORT )
		{
			HWND hList = GetDlgItem( hDlg, IDC_PORTLIST );

			int i = ListView_GetNextItem( hList, -1, LVNI_SELECTED );

			if ( ( i != -1 ) && ( MessageBox( hDlg, L"Are you sure you want to delete this port?", L"NUTS Port Manager", MB_ICONQUESTION | MB_YESNO ) == IDYES ) )
			{
				ListView_DeleteItem ( hList, i );

				std::vector<DevicePort> ports = PortConfig.EnumeratePorts();

				std::vector<ConfiguredDevicePort> Config = PortConfig.GetConfiguration();

				int j = 0;

				for ( std::vector<ConfiguredDevicePort>::iterator iCPort = Config.begin(); iCPort != Config.end(); iCPort++ )
				{
					for ( std::vector<DevicePort>::iterator iPort = ports.begin(); iPort != ports.end(); iPort++ )
					{
						if ( ( iPort->Index == iCPort->Index ) && ( iPort->Type == iCPort->Type ) )
						{
							if ( j == i )
							{
								PortConfig.DeletePort( j );
							}

							j++;
						}
					}
				}

				PortConfig.StoreConfiguration();
			}
		}

		break;

	case WM_NOTIFY:
		{
			NMHDR *pnm = (NMHDR *) lParam;

			if ( pnm->hwndFrom == GetDlgItem( hDlg, IDC_PORTLIST ) )
			{
				int i = ListView_GetNextItem( GetDlgItem( hDlg, IDC_PORTLIST ), -1, LVNI_SELECTED );

				EnableWindow( GetDlgItem( hDlg, IDC_REMOVE_PORT ), !(i == -1) );
			}
		}
		break;
	}

	return (INT_PTR) FALSE;
}

void PortManager::ConfigurePorts( void )
{
	DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_PORTMANAGER ), hMainWnd, PortConfigFunc, NULL );
}

// See: https://stackoverflow.com/questions/1388871/how-do-i-get-a-list-of-available-serial-ports-in-win32
std::vector<DevicePort>  PortManager::EnumeratePorts( void )
{
    std::vector<DevicePort> result;
	std::map< int, bool> serialmap, parallelmap;

    HRESULT hres;

    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance( CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc );

    if ( !SUCCEEDED( hres ) )
	{
		return result;
	}

    IWbemServices *pSvc = NULL;

    // Connect to the root\cimv2 namespace with the current user and obtain pointer pSvc to make IWbemServices calls.
    hres = pLoc->ConnectServer(
            bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
            NULL,                    // User name. NULL = current user
            NULL,                    // User password. NULL = current
            0,                       // Locale. NULL indicates current
            NULL,                    // Security flags.
            0,                       // Authority (for example, Kerberos)
            0,                       // Context object
            &pSvc                    // pointer to IWbemServices proxy
            );

    if ( !SUCCEEDED( hres ) )
	{
		pLoc->Release();
		
		return result;
	}

	// (RG - WTF is a "Proxy Blanket" !?!?! )
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );

    if ( !SUCCEEDED( hres ) )
	{
		pSvc->Release();
		pLoc->Release();

		return result;
	}

	// Use Win32_PnPEntity to find actual serial ports and USB-SerialPort devices
	// This is done first, because it also finds some com0com devices, but names are worse
	IEnumWbemClassObject* pEnumerator = NULL;

	hres = pSvc->ExecQuery(
		bstr_t( L"WQL" ),
		bstr_t( L"SELECT Name FROM Win32_PnPEntity WHERE Name LIKE '%(COM%' OR Name LIKE '%(LPT%'" ),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator
	);

	if ( SUCCEEDED( hres ) )
	{
		const size_t max_ports = 30;

		IWbemClassObject *pclsObj[ max_ports ] = {};

		ULONG uReturn = 0;

		do
		{
			hres = pEnumerator->Next( WBEM_INFINITE, max_ports, pclsObj, &uReturn );

			if ( SUCCEEDED( hres ) )
			{
				for ( ULONG jj = 0; jj < uReturn; jj++ )
				{
					VARIANT vtProp;

					pclsObj[ jj ]->Get( L"Name", 0, &vtProp, 0, 0 );

					// Name should be for example "Communications Port (COM1)"

					const std::wstring deviceName = vtProp.bstrVal;
					const std::wstring prefix1 = L"(COM";
					const std::wstring prefix2 = L"(LPT";

					// COM ports first
					size_t ind = deviceName.find( prefix1 );

					if ( ind != std::wstring::npos )
					{
						std::wstring nbr;

						for ( size_t i = ind + prefix1.length(); i < deviceName.length() && isdigit( deviceName[ i ] ); i++ )
						{
							nbr += deviceName[ i ];
						}

						try {
							const int portNumber = std::stoi( nbr, nullptr, 10 );

							if ( serialmap.find( portNumber ) == serialmap.end() )
							{
								serialmap[ portNumber ] = true;
								
								DevicePort port = { portNumber, PortTypeSerial, deviceName };

								result.push_back( port );
							}
						}

						catch (...)
						{
							// Don't care
						}
					}

					// Now LPT ports
					ind = deviceName.find (prefix2 );

					if ( ind != std::wstring::npos )
					{
						std::wstring nbr;

						for ( size_t i = ind + prefix2.length(); i < deviceName.length() && isdigit( deviceName[ i ] ); i++ )
						{
							nbr += deviceName[ i ];
						}

						try {
							const int portNumber = std::stoi( nbr, nullptr, 10 );

							if ( parallelmap.find( portNumber ) == parallelmap.end() )
							{
								parallelmap[ portNumber ] = true;
								
								DevicePort port = { portNumber, PortTypeParallel, deviceName };

								result.push_back( port );
							}
						}

						catch (...)
						{
							// Don't care
						}
					}

					VariantClear(&vtProp);

					pclsObj[ jj ]->Release();
				}
			}
		}
		while ( hres == WBEM_S_NO_ERROR );

		pEnumerator->Release();
	}

    // Use Win32_SerialPort to find physical ports and com0com virtual ports
    // This is more reliable, because address doesn't have to be parsed from the name

	// Serial ports First
	pEnumerator = NULL;

	hres = pSvc->ExecQuery(
		bstr_t( L"WQL" ),
		bstr_t( L"SELECT DeviceID, Name FROM Win32_SerialPort" ),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator
	);

	if ( SUCCEEDED( hres ) )
	{
		const size_t max_ports = 30;

		IWbemClassObject *pclsObj[ max_ports ] = { };

		ULONG uReturn = 0;

		do
		{
			hres = pEnumerator->Next( WBEM_INFINITE, max_ports, pclsObj, &uReturn );

            if ( SUCCEEDED( hres ) )
			{
                for ( ULONG jj = 0; jj < uReturn; jj++ )
				{
                    VARIANT vtProp1, vtProp2;

                    pclsObj[ jj ]->Get( L"DeviceID", 0, &vtProp1, 0, 0 );
                    pclsObj[ jj ]->Get( L"Name",     0, &vtProp2, 0, 0 );

                    const std::wstring deviceID = vtProp1.bstrVal;

                    if ( deviceID.substr(0, 3) == L"COM" )
					{
                        const int portNumber = std::stoi( deviceID.substr( 3 ), nullptr, 10 );

                        const std::wstring deviceName = vtProp2.bstrVal;

						if ( serialmap.find( portNumber ) == serialmap.end() )
						{
							serialmap[ portNumber ] = true;
								
							DevicePort port = { portNumber, PortTypeSerial, deviceName };

							result.push_back( port );
						}
                    }

                    VariantClear( &vtProp1 );
                    VariantClear( &vtProp2 );

                    pclsObj[ jj ]->Release();
                }
            }
        }
		while ( hres == WBEM_S_NO_ERROR );

        pEnumerator->Release();
    }

	// Then parallel ports
	pEnumerator = NULL;

	hres = pSvc->ExecQuery(
		bstr_t( L"WQL" ),
		bstr_t( L"SELECT DeviceID, Name FROM Win32_ParallelPort" ),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator
	);

	if ( SUCCEEDED( hres ) )
	{
		const size_t max_ports = 30;

		IWbemClassObject *pclsObj[ max_ports ] = { };

		ULONG uReturn = 0;

		do
		{
			hres = pEnumerator->Next( WBEM_INFINITE, max_ports, pclsObj, &uReturn );

            if ( SUCCEEDED( hres ) )
			{
                for ( ULONG jj = 0; jj < uReturn; jj++ )
				{
                    VARIANT vtProp1, vtProp2;

                    pclsObj[ jj ]->Get( L"DeviceID", 0, &vtProp1, 0, 0 );
                    pclsObj[ jj ]->Get( L"Name",     0, &vtProp2, 0, 0 );

                    const std::wstring deviceID = vtProp1.bstrVal;

                    if ( deviceID.substr(0, 3) == L"LPT" )
					{
                        const int portNumber = std::stoi( deviceID.substr( 3 ), nullptr, 10 );

                        const std::wstring deviceName = vtProp2.bstrVal;

						if ( parallelmap.find( portNumber ) == parallelmap.end() )
						{
							parallelmap[ portNumber ] = true;
								
							DevicePort port = { portNumber, PortTypeParallel, deviceName };

							result.push_back( port );
						}
                    }

                    VariantClear( &vtProp1 );
                    VariantClear( &vtProp2 );

                    pclsObj[ jj ]->Release();
                }
            }
        }
		while ( hres == WBEM_S_NO_ERROR );

        pEnumerator->Release();
    }

	pSvc->Release();
    pLoc->Release();

    return result;
}

void PortManager::ReadConfiguration( void )
{
	Config.clear();

	std::wstring PortConfigs = Preference( L"PortConfig", L"" );

	std::vector<std::wstring> ports = StringSplit( PortConfigs, L";" );

	for ( std::vector<std::wstring>::iterator iport = ports.begin(); iport != ports.end(); iport++ )
	{
		std::vector<std::wstring> portparts = StringSplit( *iport, L"," );

		if ( portparts.size() != 3 )
		{
			continue;
		}

		ConfiguredDevicePort dp;

		dp.Index = std::stoi( portparts[ 1 ], nullptr, 10 );
		dp.PLID  = portparts[ 2 ];
		dp.Name  = FSPlugins.GetProviderNameByID( dp.PLID );
		dp.Type  = PortTypeSerial;

		if ( portparts[ 0 ] == L"P" ) { dp.Type = PortTypeParallel; }

		if ( dp.Name != L"" )
		{
			Config.push_back( dp );
		}
	}
}

void PortManager:: StoreConfiguration( void )
{
	std::wstring ConfigString = L"";

	bool FirstEntry = true;

	for ( std::vector<ConfiguredDevicePort>::iterator iPort = Config.begin(); iPort != Config.end(); iPort++ )
	{
		if ( !FirstEntry )
		{
			ConfigString += L";";
		}

		std::wstring PT = L"S";

		if ( iPort->Type == PortTypeParallel ) { PT = L"P"; }

		std::wstring PortString = PT + L"," + std::to_wstring( (QWORD) iPort->Index ) + L"," + iPort->PLID;

		ConfigString += PortString;
	}

	Preference( L"PortConfig" ) = ConfigString;
}