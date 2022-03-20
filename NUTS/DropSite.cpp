#include "StdAfx.h"
#include "DropSite.h"

#include "libfuncs.h"
#include "NUTSMessages.h"

#include <ShlObj.h>
#include <vector>
#include <string>

DropSite::DropSite( HWND hWnd )
{
	RegisterDragDrop( hWnd, ( IDropTarget *) this );

	hTarget = hWnd;

	m_cRef = 1;

	AllowsDrop = false;
}


DropSite::~DropSite(void)
{
}

void DropSite::Revoke( void )
{
	RevokeDragDrop( hTarget );
}

HRESULT DropSite::QueryInterface( REFIID riid, LPVOID *ppvObj )
{
    if ( !ppvObj )
        return E_INVALIDARG;

    *ppvObj = NULL;

    if ( riid == IID_IUnknown || riid == IID_IDropTarget )
    {
        *ppvObj = (LPVOID) this;

        AddRef();

        return NOERROR;
    }

    return E_NOINTERFACE;
}

ULONG DropSite::AddRef()
{
    InterlockedIncrement( &m_cRef );

    return m_cRef;
}

ULONG DropSite::Release()
{
    ULONG ulRefCount = InterlockedDecrement( &m_cRef );

    if ( m_cRef == 0)
    {
        delete this;
    }

    return ulRefCount;
}

HRESULT DropSite::DragEnter( IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
{
	OutputDebugStringA( "External drop incoming\n" );

	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

	AllowsDrop = ( pDataObj->QueryGetData( &fmtetc ) == S_OK );

	if ( AllowsDrop )
	{
		SetFocus( hTarget );

		*pdwEffect = DROPEFFECT_COPY;
	}
	else
	{
		*pdwEffect = DROPEFFECT_NONE;
	}

	return S_OK;
}

HRESULT DropSite::DragOver( DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
{
	OutputDebugStringA( "External drop is hovering\n" );

	if ( AllowsDrop )
	{
		SetFocus( hTarget );

		*pdwEffect = DROPEFFECT_COPY;
	}
	else
	{
		*pdwEffect = DROPEFFECT_NONE;
	}

	return S_OK;
}

HRESULT DropSite::DragLeave( )
{
	OutputDebugStringA( "External drop is cancelled or leaving\n" );

	return S_OK;
}

HRESULT DropSite::Drop( IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
{
	OutputDebugStringA( "External drop has executed\n" );

	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed;

	if( pDataObj->QueryGetData( &fmtetc ) == S_OK )
	{
		if( pDataObj->GetData( &fmtetc, &stgmed ) == S_OK)
		{
			PVOID data = GlobalLock( stgmed.hGlobal );

			DROPFILES *pFiles = (DROPFILES *) data;

			/* We need to parse the DROPFILES structure to get the list of files/directories being dropped.

			   pFiles is an offset in bytes from the start of the structure to the filename list, but fWide
			   tells us if it is in WCHARs or chars. Since the originating program could be either, we must
			   handle both.
			*/

			/* This will get passed to the NUTS main window, which will then free it after convering to an AppAction */
			std::vector<std::wstring> *Paths = new std::vector<std::wstring>; 

			BYTE  *pChars = (BYTE *)  pFiles; pChars += pFiles->pFiles;
			WCHAR *pWides = (WCHAR *) pChars;

			std::string  narrowStr = "";
			std::wstring wideStr   = L"";

			while ( 1 ) /* List is double-0 terminated */
			{
				if ( pFiles->fWide )
				{
					if ( *pWides == 0 )
					{
						if ( wideStr.length() == 0 )
						{
							break;
						}
						else
						{
							Paths->push_back( wideStr );

							wideStr = L"";
						}
					}
					else
					{
						wideStr.push_back( *pWides );
					}

					pWides++;
				}
				else
				{
					if ( *pChars == 0 )
					{
						if ( narrowStr.length() == 0 )
						{
							break;
						}
						else
						{
							/* Do the unicode conversion here */
							Paths->push_back( std::wstring( UString( (char *) narrowStr.c_str() ) ) );

							narrowStr = "";
						}
					}
					else
					{
						narrowStr.push_back( *pChars );
					}

					pChars++;
				}
			}

			::PostMessage( hTarget, WM_EXTERNALDROP, (WPARAM) hTarget, (LPARAM) Paths );

			GlobalUnlock(stgmed.hGlobal);

			ReleaseStgMedium( &stgmed );
		}
	}
	
	return S_OK;
}