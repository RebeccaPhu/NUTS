#include "StdAfx.h"
#include "BASICContentViewer.h"
#include <richedit.h>
#include <stdio.h>
#include "resource.h"

std::map<HWND,CBASICContentViewer *> CBASICContentViewer::viewers;

bool CBASICContentViewer::WndClassReg = false;

LRESULT CALLBACK CBASICContentViewer::BCViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( viewers.find( hWnd ) != viewers.end() )
	{
		return viewers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

CBASICContentViewer::CBASICContentViewer( CTempFile FileObj )
{
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= CBASICContentViewer::BCViewerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_BASIC));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL; // MAKEINTRESOURCE(IDC_NUTS);
		wcex.lpszClassName	= L"NUTS BASIC Program Renderer";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_BASIC));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	lContent = FileObj.Ext();
	pContent = (BYTE *) malloc( (size_t) lContent );

	FileObj.Seek( 0 );
	FileObj.Read( pContent, (DWORD) lContent );
}

CBASICContentViewer::~CBASICContentViewer(void) {
}

int CBASICContentViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	hWnd = CreateWindowEx(
		NULL,
		L"NUTS BASIC Program Renderer",
		L"NUTS BASIC Program Renderer",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
		Parent, NULL, hInstance, NULL
	);

	CBASICContentViewer::viewers[ hWnd ] = this;

	ParentWnd = Parent;

	RECT rect;

	GetClientRect(hWnd, &rect);

	hText = CreateWindowEx(
		ES_MULTILINE | ES_READONLY,
		RICHEDIT_CLASS, NULL,
		WS_VSCROLL | ES_AUTOVSCROLL | ES_READONLY | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
		0,0, rect.right -rect.left, rect.bottom-rect.top, hWnd, NULL, hInst, NULL
	);

	ShowWindow(hWnd, TRUE);
	UpdateWindow(hWnd);

	DeTokenise();

	return 0;
}

LRESULT	CBASICContentViewer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {


	return DefWindowProc( hWnd, message, wParam, lParam);
}

void CBASICContentViewer::DeTokenise() {
	static char *tokens[] = {
		"AND", "DIV", "EOR", "MOD", "OR", "ERROR", "LINE", "OFF", "STEP", "SPC", "TAB(", "ELSE", "THEN", "",
		"OPENIN", "PTR", "PAGE", "TIME", "LOMEM", "HIMEM", "ABS", "ACS", "ADVAL", "ASC", "ASN", "ATN", "BGET", "COS", "COUNT",
		"DEG", "ERL", "ERR", "EVAL", "EXP", "EXT", "FALSE", "FN", "GET", "INKEY", "INSTR(", "INT", "LEN", "LN", "LOG",
		"NOT", "OPENUP", "OPENOUT", "PI", "POINT(", "POS", "RAD", "RND", "SGN", "SIN", "SQR", "TAN", "TO", "TRUE", "USR",
		"VAL", "VPOS", "CHR$", "GET$", "INKEY$", "LEFT$(", "MID$(", "RIGHT$(", "STR$", "STRING$(", "", "", "",
		"LIST", "NEW", "OLD", "RENUMBER", "SAVE", "EDIT", "PUT", "PTR", "PAGE", "TIME", "LOMEM", "HIMEM", "SOUND",
		"BPUT", "CALL", "CHAIN", "CLEAR", "CLOSE", "CLG", "CLS", "DATA", "DEF", "DIM", "DRAW", "END", "ENDPROC", "ENVELOPE",
		"FOR", "GOSUB", "GOTO", "GCOL", "IF", "INPUT", "LET", "LOCAL", "MODE", "MOVE", "NEXT", "ON", "VDU", "PLOT", "PRINT",
		"PROC", "READ", "REM", "REPEAT", "REPORT", "RESTORE", "RETURN", "RUN", "STOP", "COLOUR", "TRACE", "UNTIL", "WIDTH", "OSCLI"
	};

	lTextBuffer	= 32768;

	pTextBuffer		= (char *) malloc(32768);

	char	line[8192];

	int	i	= 0;
	int	o	= 0;
	int	p	= 0;

	int	s	= 0;
	int	l	= 0;
	int	q	= 0;

	WCHAR	*result;

	while (1) {
		switch (s) {	// state
			case 0:		// Process new input
				if (pContent[i] == 0x0d) {
					s	= 1;
					l	= 0;
					p	= 0;
				}

				break;

			case 1:		// Line number
				if ((pContent[i]	== 0xff) && (p == 0)) {
					pTextBuffer[o]	= 0;

					result	= (WCHAR *) malloc((strlen(pTextBuffer) + 10) * sizeof(WCHAR));

					MultiByteToWideChar(GetACP(), NULL, pTextBuffer, strlen(pTextBuffer) + 1, result, strlen(pTextBuffer) + 1);

					SendMessage(hText, WM_SETTEXT, 0, (LPARAM) result);

					free(result);

					return;
				}

				l	*= 256;	l += pContent[i];

				p++;

				if (p == 2) {
					p	= 5;
					
					sprintf_s(line, 8192, "%5d", l);

					s	= 2;
				}

				break;

			case 2:	// line length
				s	= 3;
				q	= 0;
				p	= 5;

				break;

			case 3:	// line data
				if (pContent[i] == 0x0d) {
					s	= 0;
					i--;

					line[p]	= 0;

					while ((o + (int) strlen(line) + 3) > lTextBuffer) {
						lTextBuffer	+= 32768;

						pTextBuffer	= (char *) realloc(pTextBuffer, lTextBuffer);
					}

					memcpy(&pTextBuffer[o], line, strlen(line));
					
					o	+= strlen(line);

					pTextBuffer[o]	= 13;	o++;
					pTextBuffer[o]	= 10;	o++;
				} else if ((pContent[i] == 0x8d) && (q == 0)) {
					l	= 0;

					l	|= ((pContent[i + 1] ^ 0x54) & 0x30) << 2;
					l	|= ((pContent[i + 1] ^ 0x54) & 0x03) << 12;

					l	|= (pContent[i + 3] & 0x3f) << 16;

					l	|= pContent[i + 2] & 0x3f;

					i	+= 3;

					sprintf_s(&line[p], 8192 - p, "%d", l);

					p	+= strlen(&line[p]) - 1;
				} else {
					if (pContent[i] == '\"')
						q	= 1 - q;

					if ((pContent[i] > 0x7f) && (q == 0)) {
						memcpy(&line[p], tokens[pContent[i] - 0x80], strlen(tokens[pContent[i] - 0x80]));

						p	+= strlen(tokens[pContent[i] - 0x80]) - 1;
					} else
						line[p]	= pContent[i];
				}

				p++;

				break;
		}

		i++;

	}
}

