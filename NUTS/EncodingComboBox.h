#pragma once

#include "EncodingEdit.h"
#include "FontBitmap.h"
#include "Defs.h"

#include <vector>
#include <map>

class EncodingComboBox
{
public:
	EncodingComboBox( HWND hParent, int x, int y, int w );
	~EncodingComboBox( void );

	static std::map<HWND, EncodingComboBox *> boxes;
	static bool _HasWindowClass;

	LRESULT WindowProc( HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

public:
	HWND hWnd;
	HWND hDropDown;
	bool SelectOnly;

private:
	HWND ParentWnd;

	EncodingEdit *pTextArea;

public:
	BYTE *GetText() {
		return pTextArea->GetText();
	}

	long GetHexText() {
		return pTextArea->GetHexText();
	}

	long GetDecText() {
		return pTextArea->GetDecText();
	}

	long GetOctText() {
		return pTextArea->GetOctText();
	}

	void AddTextItem( BYTE *pItem )
	{
		ListEntries[ iIndex++ ] = rstrndup( pItem, 64 );
	}

	void EnableCombo( void )
	{
		SelectOnly = false;

		pTextArea->Disabled = false;
	}

	void SetSel( WORD Item )
	{
		if ( ( Item < ListEntries.size() ) && ( SelectOnly ) )
		{
			pTextArea->SetText( ListEntries[ Item ] );
		}
	}

	void SetSelText( BYTE *t )
	{
		pTextArea->SetText( t );
	}

	void SetAllowed( EncodingEdit::InputType it )
	{
		pTextArea->AllowedChars = it;
	}

	void SetMaxLen( BYTE Len )
	{
		pTextArea->MaxLen = Len;
	}

private:
	int  DoPaint( HWND wnd );
	int  DoDropdown( void );
	void TrackMouse( void );

private:
	bool  DDPressed;
	DWORD iIndex;
	bool  Tracking;
	bool  OverDropdown;
	long  HoverID;
	DWORD wx;
	DWORD wy;
	DWORD ww;

	std::map< WORD, BYTE *> ListEntries;
};

