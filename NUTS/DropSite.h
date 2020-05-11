#pragma once

#include <ShellAPI.h>
#include <OleIdl.h>

class DropSite : public IDropTarget
{
public:
	DropSite( HWND hWnd );
	~DropSite(void);

public:
	void Revoke( void );

public: // IUnknown
	STDMETHODIMP QueryInterface( REFIID riid, LPVOID *ppvObj );
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

public: // IDropTarget
	STDMETHODIMP DragEnter( IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect );
	STDMETHODIMP DragOver( DWORD grfKeyState, POINTL pt, DWORD *pdwEffect );
	STDMETHODIMP DragLeave( );
	STDMETHODIMP Drop( IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect );

private:
	ULONG m_cRef;

	HWND hTarget;

	bool AllowsDrop;
};

