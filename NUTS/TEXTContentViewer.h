#pragma once

#include "TEXTTranslator.h"
#include "TempFile.h"
#include "EncodingTextArea.h"
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
	HWND   hWnd;

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

	BYTE  *pTextBuffer;
	long  lTextBuffer;

	DWORD XlatorID;

	TEXTTranslator *pXlator;

	std::string TempPath;

	DWORD  dwthreadid;
	HANDLE hTranslateThread;
	HANDLE hStopEvent;
	HWND   hProgress;

	bool Translating;

private:
	void Translate( void );
	void UpdateText( void );
	void DoResize();

};
