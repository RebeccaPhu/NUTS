#pragma once

#include <map>
#include "SCREENTranslator.h"
#include "IconButton.h"

class CPaletteWindow
{
public:
	CPaletteWindow(void);
	~CPaletteWindow(void);

public:
	void Create(int x, int y, HWND ParentWnd, LogPalette *pLog, PhysPalette *pPhys, PhysColours *pCols);

	LRESULT	WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR LogicalWindowProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

	static LRESULT CALLBACK PaletteWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ColourAreaWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK LogWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static std::map<HWND, CPaletteWindow *> WndMap;
	static bool WndReg;

public:
	HWND Parent;
	HWND hWnd;
	HWND hColourArea;

	LogPalette *pLogical;
	PhysPalette *pPhysical;
	PhysColours *pColours;

private:
	WORD sqrLogical;
	WORD sqrPhysical;
	WORD sqrBiggest;
	WORD sqrColours;
	WORD TotalColourSlots;

	bool FlashPhase;

	HWND hTabs;
	HWND hPhysDlg;
	
	IconButton *pResetButton;
	IconButton *pSaveButton;
	IconButton *pLoadButton;

	WORD ColIndex;
	bool ColIn;

	WORD PColIndex;
	bool PColIn;

	typedef enum _PaletteTab {
		TabLogical  = 0,
		TabPhysical = 1
	} PaletteTab;

	PaletteTab SelectedTab;

private:
	void ComputeWindowSize( void );
	void DoLogicalPaletteChange( void );
	void DoPhysicalPaletteChange( void );
	void PaintColours( void );
	void PaintPhysicalColours( HWND hDlg );
	void DoSavePalette( void );
	void DoLoadPalette( void );

	void DestroyWindows( void );
};
