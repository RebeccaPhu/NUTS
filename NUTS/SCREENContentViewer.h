#pragma once

#include "PaletteWindow.h"
#include "Defs.h"
#include "resource.h"
#include "TempFile.h"
#include "SCREENTranslator.h"
#include "EncodingEdit.h"
#include "IconButton.h"

#include <map>

class CSCREENContentViewer
{
public:
	CSCREENContentViewer( CTempFile &FileObj, DWORD TUID );
	~CSCREENContentViewer(void);

public:
	int Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h);

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	static std::map<HWND, CSCREENContentViewer *> viewers;
	static bool WndClassReg;
	static LRESULT CALLBACK SCViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static unsigned int __stdcall CSCREENContentViewer::_TranslateThread(void *param);

public:
	HWND hWnd;

private:
	HWND ParentWnd;
	HWND hToolbar;
	HWND hModeList;
	IconButton *pPaletteButton;
	HWND hEffects;
	IconButton *pCopy;
	IconButton *pSave;
	IconButton *pPrint;
	HWND hOffsetPrompt;
	HWND hLengthPrompt;
	HWND hProgress;

	HWND hModeTip;
	HWND hEffectsTip;
	HWND hOffsetTip;
	HWND hLengthTip;

	EncodingEdit *pOffset;
	EncodingEdit *pLength;

	int  Mode;
	bool bAntiAlias;
	bool Flash;
	bool FirstTranslate;

	QWORD lContent;
	QWORD Length;
	DWORD *pixels1;
	DWORD *pixels2;

	long long Offset;

	BITMAPINFO	*bmi;

	CPaletteWindow	*palWnd;

	SCREENTranslator *pXlator;
	DWORD            XlatorID;
	std::wstring     Path;

	typedef enum _EffectID {
		Effect_Antialias = 0x00000001,
		Effect_CRTGlow   = 0x00000002,
		Effect_Scanlines = 0x00000004,
		Effect_Snow      = 0x00000008,
		Effect_Ghosting  = 0x00000010,
		Effect_Rainbow   = 0x00000020,
	} EffectID;

	DWORD Effects;

	DWORD AspectWidth;
	DWORD AspectHeight;
	DWORD ww;
	DWORD wh;

	HANDLE hTranslateThread;
	HANDLE hTerminate;
	HANDLE hPoke;

	bool   Translating;

private:
	int Translate( void );
	int CreateToolbar( void );
	int PaintToolBar( void );
	int DisplayImage( void );
	int DoEffectsMenu( void );
	int DoCopyImage( bool Save );
	int DoPrintImage( void );

	LogPalette  LogicalPalette;
	PhysPalette PhysicalPalette;
	PhysColours PhysicalColours;

	void Upscale( DWORD **pPixels, BITMAPINFO *pBMI, bool UpdateBMI );

	void DoScanlines( DWORD **pPixels, BITMAPINFO *pBMI );
	void DoAntialias( DWORD **pPixels, BITMAPINFO *pBMI );
	void DoSnow( DWORD **pPixels, BITMAPINFO *pBMI );
	void DoRainbow( DWORD **pPixels, BITMAPINFO *pBMI );
	void DoGhosting( DWORD **pPixels, BITMAPINFO *pBMI );
	void DoEffectMultiplier( int DoneEffects, DWORD **pPixels, BITMAPINFO *pBMI );

	float rgb2luma( DWORD *pix );

	void DestroyWindows( void );

	int  TranslateThread( void );
};
