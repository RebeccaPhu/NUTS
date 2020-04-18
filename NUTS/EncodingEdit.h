#pragma once

#include "stdafx.h"
#include "Defs.h"

#include <map>

class EncodingEdit
{
public:
	EncodingEdit( HWND hParent, int x, int y, int w, bool FontChanger = false );
	~EncodingEdit(void);

public:
	typedef enum _InputType
	{
		textInputAny = 1,
		textInputHex = 2,
		textInputOct = 3,
		textInputDec = 4,
		textInputBin = 5,
	} InputType;

public:
	static std::map<HWND, EncodingEdit *> _EncodingEditClassMap;
	static bool _HasWindowClass;

	DWORD Encoding;
	HWND  hWnd;
	bool  Disabled;
	bool  SoftDisable;
	BYTE  MaxLen;
	bool  AllowNegative;

	InputType AllowedChars;

public:
	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	void SetText( BYTE *pText );
	BYTE *GetText( void ) { EditText[ Length ] = 0; return EditText; };
	long GetHexText( void );
	long GetOctText( void );
	long GetDecText( void );
	void SelectAll( void );
	void SetFocus( void );

private:
	HWND  hChanger;
	HWND  Parent;
	HICON hIcon;
	BYTE  Blink;

	BYTE  EditText[ 256 ];
	BYTE  Length;
	BYTE  Cursor;
	BYTE  DChars;
	BYTE  SChar;
	bool  HasFocus;
	bool  LDown;
	bool  Shift;
	bool  Ctrl;
	int   CW;
	BYTE  SCursor;
	BYTE  ECursor;
	bool  Select;
	bool  Changes;
	WORD  MouseXS;

	std::vector<DWORD> FontSelection;
	WORD               FontNum;

	HBRUSH hDisBrush;

private:
	void PaintControl( void );
	void Invalidate( void );
};

