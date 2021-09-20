#pragma once

#include "TEXTTranslator.h"
#include "TempFile.h"
#include "EncodingTextArea.h"
#include "EncodingStatusBar.h"
#include "Defs.h"

#include <map>

class CTEXTContentViewer
{
public:
	CTEXTContentViewer( CTempFile &FileObj, DWORD TUID );
	~CTEXTContentViewer(void);

public:
	int Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h);

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	HWND  hWnd;
	HWND  hChanger;
	HWND  hCopy;
	HWND  hSave;
	HWND  hPrint;

	BYTE  *pContent;
	QWORD lContent;
	DWORD FSEncodingID;

public:
	static LRESULT CALLBACK TEXTViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static std::map<HWND,CTEXTContentViewer *> viewers;
	static bool WndClassReg;

	static unsigned int __stdcall TranslateThread(void *param);

private:
	HWND  ParentWnd;
	
	EncodingTextArea *pTextArea;
	EncodingStatusBar *pStatusBar;

	BYTE  *pTextBuffer;
	long  lTextBuffer;

	DWORD XlatorID;

	TEXTTranslator *pXlator;

	std::wstring TempPath;

	std::vector<DWORD> FontList;

	DWORD  FontNum;

	DWORD  dwthreadid;
	HANDLE hTranslateThread;
	HANDLE hStopEvent;
	HWND   hProgress;

	bool   Translating;
	bool   Retranslate;

	DWORD  ResizeTime;

	TXTTranslateOptions opts;

private:
	void Translate( void );
	void DoResize();
	int  CreateToolbar( void );
	int  PaintToolBar( void );
	void DoSave( void );
	void DoPrint( void );
	void BeginTranslate( void );
	void RetranslateCheck( void );
};
