#include "stdafx.h"

#include "FileDialogs.h"

#include <CommDlg.h>
#include <string>

bool SaveFileDialog( HWND hWnd, std::wstring &filename, std::wstring FileType, std::wstring Ext, std::wstring Caption )
{
	WCHAR Filename[ MAX_PATH + 4 ];

	Filename[ 0 ] = 0;

	OPENFILENAME of;

	ZeroMemory( &of, sizeof( of ) );

	WCHAR filter[ 256 ];
	wsprintf( &filter[ 0 ], L"%s", FileType.c_str() );
	wsprintf( &filter[ FileType.length() + 1 ], L"*.%s", Ext.c_str() );

	filter[ FileType.length() + Ext.length() + 2 ] = 0;

	of.lStructSize       = sizeof( OPENFILENAME );
	of.hwndOwner         = hWnd;
	of.lpstrCustomFilter = NULL;
	of.lpstrFile         = Filename;
	of.nMaxFile          = MAX_PATH - 1;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.lpstrInitialDir   = NULL;
	of.nFilterIndex      = 1;
	of.Flags             = OFN_OVERWRITEPROMPT;
	of.lpstrFilter       = filter;
	of.lpstrTitle        = Caption.c_str();

	bool r = GetSaveFileName( &of );

	if ( r )
	{
		filename = std::wstring( Filename );

		if ( filename.length() > Ext.length() + 1 )
		{
			if ( filename.substr( filename.length() - Ext.length() - 1 ) != std::wstring( L"." + Ext ) )
			{
				filename = filename + L"." + Ext;
			}
		}
		else
		{
			filename = filename + L"." + Ext;
		}
	}

	return r;
}

bool OpenFileDialog( HWND hWnd, std::wstring &filename, std::wstring FileType, std::wstring Ext, std::wstring Caption )
{
	WCHAR Filename[ MAX_PATH ];

	Filename[ 0 ] = 0;

	OPENFILENAME of;

	ZeroMemory( &of, sizeof( of ) );

	WCHAR filter[ 256 ];
	wsprintf( &filter[ 0 ], L"%s", FileType.c_str() );
	wsprintf( &filter[ FileType.length() + 1 ], L"*.%s", Ext.c_str() );

	filter[ FileType.length() + Ext.length() + 2 + 2 ] = 0;

	of.lStructSize       = sizeof( OPENFILENAME );
	of.hwndOwner         = hWnd;
	of.lpstrCustomFilter = NULL;
	of.lpstrFile         = Filename;
	of.nMaxFile          = MAX_PATH;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.lpstrInitialDir   = NULL;
	of.nFilterIndex      = 1;
	of.Flags             = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	of.lpstrFilter       = filter;
	of.lpstrTitle        = Caption.c_str();

	bool r = GetOpenFileName( &of );

	if ( r )
	{
		filename = std::wstring( Filename );
	}

	return r;
}

bool OpenAnyFileDialog( HWND hWnd, std::wstring &filename, std::wstring Caption )
{
	WCHAR Filename[ MAX_PATH ];

	Filename[ 0 ] = 0;

	OPENFILENAME of;

	ZeroMemory( &of, sizeof( of ) );

	WCHAR filter[ 256 ];
	wsprintf( &filter[ 0 ], L"Any file" );
	wsprintf( &filter[ 9 ], L"*.*" );

	filter[ 13 ] = 0;

	of.lStructSize       = sizeof( OPENFILENAME );
	of.hwndOwner         = hWnd;
	of.lpstrCustomFilter = NULL;
	of.lpstrFile         = Filename;
	of.nMaxFile          = MAX_PATH;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.lpstrInitialDir   = NULL;
	of.nFilterIndex      = 1;
	of.Flags             = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	of.lpstrFilter       = filter;
	of.lpstrTitle        = Caption.c_str();

	bool r = GetOpenFileName( &of );

	if ( r )
	{
		filename = std::wstring( Filename );
	}

	return r;
}
