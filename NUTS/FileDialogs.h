#pragma once

#include <string>

bool SaveFileDialog( HWND hWnd, std::wstring &filename, std::wstring FileType, std::wstring Ext, std::wstring Caption );
bool OpenFileDialog( HWND hWnd, std::wstring &filename, std::wstring FileType, std::wstring Ext, std::wstring Caption );
bool OpenAnyFileDialog( HWND hWnd, std::wstring &filename, std::wstring Caption );