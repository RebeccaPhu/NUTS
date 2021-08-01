// TZXConfig.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "TZXConfig.h"
#include "../NUTS/FileDialogs.h"
#include "../NUTS/libfuncs.h"
#include "TZXHardware.h"

#include "resource.h"

int PickABlock( void );
int PickAString( std::wstring Prompt );
int PickHardware( void );
int PickArchive( void );

HINSTANCE hInstance;

BYTE SelectedID  = 0xFF;
int  SelectedBlk = -1;
bool IsNewBlock  = false;

CTempFile *pBlockStore = nullptr;

bool TopLevelCreated = false;

HWND SubDialog = NULL;
HWND DialogWnd = NULL;

typedef struct _MenuOption {
	WCHAR Name[ 64 ];
	DWORD Value;
} MenuOption;

DWORD MenuPickControl = 0;
MenuOption *MenuPickSelect = NULL;

std::wstring StringEntryTitle;
std::wstring StringEntryResponse;
std::wstring ArchiveString;
BYTE ArchiveField;
int StringEntryResult;
int HardwareResult;
int ArchiveResult;
int PathSelectResult;

bool ChangedDataSet;
std::wstring DataSetPath;

#define WMBASE 50000

const WCHAR *BlockPrompt[] = {
	L"Specify data source for block, or define header:",
	L"Specify data source for block, with pulse lengths:",
	L"Specify pulse length:",
	L"Specify pulse configurations:",
	L"Specify data source for block, with pulse lengths:",
	L"Specify data source for block, with pulse lengths:",
	L"Specify data source for CSW recording:",
	L"Specify data source for block, with pulse lengths:",
	L"Specify pause instruction duration (use 0 for \"Stop the tape\"):",
	L"Specify group name:",
	L"No configuration",
	L"Specify jump target:",
	L"Specify number of loops:",
	L"No configuration",
	L"Specify call targets:",
	L"No configuration",
	L"Specify selection list:",
	L"No configuration",
	L"Specify signal level:",
	L"Specify text description:",
	L"Specify text message:",
	L"Specify archive information fields:",
	L"Specify hardware information fields:",
	L"Specify data for custom block:",
	L"No configuration",
	nullptr
};

const WCHAR *GetHardwareString( DWORD HType, DWORD HID )
{
	const static WCHAR *pUnk = L"Unknown";

	if ( HType >= 0x10 ) { return pUnk; }

	WCHAR **pList = (WCHAR **) TZXHardwareTypeList[ HType ];

	if ( HID >= TZXMaxID[ HType ] ) { return pUnk; }

	return pList[ HID ];
}

std::vector<NativeFile> *pFiles = nullptr;

std::vector<WORD> Pulses;
std::vector<WORD> JumpVectors;
std::vector<std::pair<BYTE, std::wstring>> ArchiveFields;
std::vector<DWORD> HardwareFields;
std::vector<std::pair<WORD, std::wstring>> SelectFields;
std::vector<std::wstring> *pSelectPaths;


inline const int UIToInt( DWORD Ctrl )
{
	WCHAR vv[ 16 ];

	::SendMessage( GetDlgItem( SubDialog, Ctrl ), WM_GETTEXT, 15, (LPARAM) vv );

	return _wtoi( vv );
}

inline void WORDValueToBlock( DWORD Ctrl, DWORD Offset )
{
	pBlockStore->Seek( Offset + 1 );

	WORD v = UIToInt( Ctrl );

	pBlockStore->Write( &v, 2 );
}

inline void BYTEValueToBlock( DWORD Ctrl, DWORD Offset )
{
	pBlockStore->Seek( Offset + 1 );

	BYTE v = UIToInt( Ctrl );

	pBlockStore->Write( &v, 1 );
}

inline BYTE CountedStringToBlock( DWORD Ctrl, DWORD Offset )
{
	pBlockStore->Seek( Offset + 1 );

	WCHAR vv[ 256 ];

	::SendMessage( GetDlgItem( SubDialog, Ctrl ), WM_GETTEXT, 255, (LPARAM) vv );

	char *pA = AString( vv );

	BYTE r = (BYTE) rstrnlen( (BYTE *) pA, 255 );

	pBlockStore->Write( &r, 1 );
	pBlockStore->Write( pA, r );

	return r;
}

DWORD WriteDataSet( BYTE *ck = nullptr )
{
	CTempFile src( DataSetPath );

	src.Keep();

	DWORD BytesToGo = src.Ext();

	src.Seek( 0 );

	BYTE Buffer[ 16384 ];

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( 16384, BytesToGo );

		src.Read( Buffer, BytesRead );

		if ( ck != nullptr )
		{
			for ( DWORD i=0; i<BytesRead; i++ )
			{
				*ck ^= Buffer[ i ];
			}
		}

		pBlockStore->Write( Buffer, BytesRead );

		BytesToGo -= BytesRead;
	}

	return (DWORD) src.Ext();
}

void WriteEditedBlock( void )
{
	pBlockStore->Seek( 0 );
	pBlockStore->Write( &SelectedID, 1 );

	switch ( SelectedID )
	{
	case 0x10:
		{
			/* Well now. There are various options here, depending on check boxes being set. */
			bool UseHeader = false;
			bool UseFlag   = false;
			BYTE FlagByte  = 0x00;

			if ( ::SendMessage( GetDlgItem( SubDialog, IDC_DATABYTE ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { FlagByte = 0xFF; }
			if ( ::SendMessage( GetDlgItem( SubDialog, IDC_USEHEADER ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { UseHeader = true; }
			if ( ::SendMessage( GetDlgItem( SubDialog, IDC_USEFLAG ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { UseFlag = true; }

			DWORD DataSize = 0;
			WORD  HeaderSize = 17; if ( UseFlag ) { HeaderSize += 2; }
			BYTE  Header[ 17 ];
			WCHAR vv[ 64 ];

			if ( UseHeader )
			{
				/* Fix up the filename with padding */
				::SendMessage( GetDlgItem( SubDialog, IDC_FILENAME ), WM_GETTEXT, 63, (LPARAM) vv );

				memcpy( &Header[ 1 ], AString( vv ), 10 );

				BYTE n = rstrnlen( &Header[ 1 ], 10 );

				if ( n != 10 )
				{
					for ( BYTE i=n; i<10; i++ )
					{
						Header[ 1 + i ] = 0x20;
					}
				}

				/* The remaining attributes */
				BYTE PType = ::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_GETCURSEL, 0, 0 );

				Header[ 0 ] = PType;

				::SendMessage( GetDlgItem( SubDialog, IDC_DATALEN ), WM_GETTEXT, 63, (LPARAM) vv );
				* (WORD *) &Header[ 0x0B ] = _wtoi( vv );

				switch ( PType )
				{
				case 0:
					{
						::SendMessage( GetDlgItem( SubDialog, IDC_STARTLINE ), WM_GETTEXT, 63, (LPARAM) vv );

						WORD AutoStart = _wtoi( vv );

						::SendMessage( GetDlgItem( SubDialog, IDC_VARADDR ), WM_GETTEXT, 63, (LPARAM) vv );

						WORD VarAddr = _wtoi( vv );

						* (WORD *) &Header[ 0x0D ] = AutoStart;
						* (WORD *) &Header[ 0x0F ] = VarAddr;
					}
					break;

				case 1:
				case 2:
					{
						::SendMessage( GetDlgItem( SubDialog, IDC_VARIABLE ), WM_GETTEXT, 63, (LPARAM) vv );

						char *a = AString( vv );

						Header[ 0x0E ] = a[ 0 ];
					}
					break;

				case 3:
					{
						::SendMessage( GetDlgItem( SubDialog, IDC_LOADADDR ), WM_GETTEXT, 63, (LPARAM) vv );

						WORD LoadAddr = _wtoi( vv );

						* (WORD *) &Header[ 0x0D ] = LoadAddr;
						* (WORD *) &Header[ 0x0F ] = 32768;
					}
					break;
				}

			}

			::SendMessage( GetDlgItem( SubDialog, IDC_PAUSE ), WM_GETTEXT, 63, (LPARAM) vv );

			WORD Pause = _wtoi( vv );

			pBlockStore->Seek( 1 );
			pBlockStore->Write( &Pause, 2 );
			pBlockStore->Seek( 5 );

			if ( UseHeader )
			{
				if ( UseFlag )
				{
					pBlockStore->Write( &FlagByte, 1 );
				}

				pBlockStore->Write( Header, 17 );

				if ( UseFlag )
				{
					BYTE ck = FlagByte;

					for ( BYTE i=0; i<17; i++ ) { ck ^= Header[ i ]; }

					pBlockStore->Write( &ck, 1 );
				}

				pBlockStore->Seek( 3 );
				pBlockStore->Write( &HeaderSize, 2 );
				pBlockStore->SetExt( 5 + HeaderSize );
			}
			else
			{
				BYTE ck    = 0;

				if ( UseFlag )
				{
					pBlockStore->Write( &FlagByte, 1 );

					ck = FlagByte;
				}

				WORD bytes = WriteDataSet( &ck );

				if ( UseFlag )
				{
					bytes += 2;

					pBlockStore->Write( &ck, 1 );
				}

				pBlockStore->Seek( 3 );
				pBlockStore->Write( &bytes, 2 );
				pBlockStore->SetExt( 5 + bytes );
			}
		}
		break;

	case 0x11:
		WORDValueToBlock( IDC_PILOTL,   0x00 );
		WORDValueToBlock( IDC_SYNC1L,   0x02 );
		WORDValueToBlock( IDC_SYNC2L,   0x04 );
		WORDValueToBlock( IDC_ZEROL,    0x06 );
		WORDValueToBlock( IDC_ONEL,     0x08 );
		WORDValueToBlock( IDC_PILOTC,   0x0A );
		BYTEValueToBlock( IDC_USEDBITS, 0x0C );
		WORDValueToBlock( IDC_PAUSE,    0x0D );

		if ( ChangedDataSet )
		{
			pBlockStore->Seek( 0x13 );

			DWORD sz = WriteDataSet();

			pBlockStore->Seek( 0x10 );
			pBlockStore->Write( &sz, 3 );
			pBlockStore->SetExt( sz + 0x13 );
		}

		break;

	case 0x12:
		WORDValueToBlock( IDC_1NTEXT, 0x00 );
		WORDValueToBlock( IDC_1STEXT, 0x02 );

		pBlockStore->SetExt( 5 );

		break;

	case 0x13:
		{
			BYTE n = (BYTE) Pulses.size();

			pBlockStore->Write( &n, 1 );

			for ( BYTE i=0; i<n; i++ )
			{
				WORD l = Pulses[ i ];

				pBlockStore->Write( &l, 2 );
			}

			pBlockStore->SetExt( ( n * 2 ) + 2 );
		}

		break;

	case 0x14:
		WORDValueToBlock( IDC_ZEROL,    0x00 );
		WORDValueToBlock( IDC_ONEL,     0x02 );
		BYTEValueToBlock( IDC_USEDBITS, 0x04 );
		WORDValueToBlock( IDC_PAUSE,    0x05 );

		if ( ChangedDataSet )
		{
			pBlockStore->Seek( 0x0B );

			DWORD sz = WriteDataSet();

			pBlockStore->Seek( 0x08 );
			pBlockStore->Write( &sz, 3 );
			pBlockStore->SetExt( sz + 0x0B );
		}

		break;

	case 0x15:
	case 0x18:
	case 0x19:
		MessageBox( SubDialog, L"Here be dragons!", L"NUTS Dragon Warrior", MB_OK );
		
		break;

	case 0x20:
		WORDValueToBlock( IDC_1NTEXT, 0x00 );

		pBlockStore->SetExt( 3 );

		break;

	case 0x21:
	case 0x30:
		{
			BYTE b = CountedStringToBlock( IDC_1STEXT, 0x00 );

			pBlockStore->SetExt( b + 2 );
		}

		break;

	case 0x22:
	case 0x25:
	case 0x27:
		pBlockStore->SetExt( 1 );
		break;

	case 0x23:
		{
			WORD v = JumpVectors[ 0 ];

			pBlockStore->Write( &v, 2 );

			pBlockStore->SetExt( 3 );
		}
		break;

	case 0x24:
		WORDValueToBlock( IDC_1NTEXT, 0x00 );
		pBlockStore->SetExt( 3 );
		break;

	case 0x26:
		{
			WORD n = (WORD) JumpVectors.size();

			pBlockStore->Write( &n, 2 );

			for ( BYTE i=0; i<n; i++ )
			{
				WORD l = JumpVectors[ i ];

				pBlockStore->Write( &l, 2 );
			}

			pBlockStore->SetExt( ( n * 2 ) + 3 );
		}
		break;

	case 0x28:
		{
			BYTE n  = (BYTE) SelectFields.size();
			WORD BL = 0;

			pBlockStore->Seek( 3 );
			pBlockStore->Write( &n, 1 );

			for ( BYTE i=0; i<n; i++ )
			{
				WORD l         = SelectFields[ i ].first;
				std::wstring s = SelectFields[ i ].second;
				BYTE *pS       = (BYTE *) AString( (WCHAR *) s.c_str() );
				BYTE sl        = rstrnlen( pS, 255 );

				pBlockStore->Write( &l , 2 );
				pBlockStore->Write( &sl, 1 );
				pBlockStore->Write( pS, sl );

				BL += 2 + 1 + sl;
			}

			BL++;

			pBlockStore->Seek( 1 );
			pBlockStore->Write( &BL, 2 );
			pBlockStore->SetExt( BL + 3 );
		}
		break;
	
	case 0x2A:
		{
			DWORD Size = 0x00000000;

			pBlockStore->Write( &Size, 4 );
			pBlockStore->SetExt( 5 );
		}
		break;

	case 0x2B:
		{
			DWORD Size = 0x00000001;

			pBlockStore->Write( &Size, 4 );

			BYTE sig = 0;
			
			if ( ::SendMessage( GetDlgItem( SubDialog, IDC_HIGHRADIO ), BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			{
				sig = 0x01;
			}

			pBlockStore->Write( &sig, 1 );

			pBlockStore->SetExt( 6 );
		}
		break;

	case 0x31:
		{
			BYTEValueToBlock( IDC_1NTEXT, 0x00 );

			BYTE b = CountedStringToBlock( IDC_1STEXT, 0x01 );

			pBlockStore->SetExt( b + 3 );
		}
		break;

	case 0x32:
		{
			BYTE n  = (BYTE) ArchiveFields.size();
			WORD BL = 0;

			pBlockStore->Seek( 3 );
			pBlockStore->Write( &n, 1 );

			for ( BYTE i=0; i<n; i++ )
			{
				BYTE l         = ArchiveFields[ i ].first;
				std::wstring s = ArchiveFields[ i ].second;
				BYTE *pS       = (BYTE *) AString( (WCHAR *) s.c_str() );
				BYTE sl        = rstrnlen( pS, 255 );

				pBlockStore->Write( &l , 1 );
				pBlockStore->Write( &sl, 1 );
				pBlockStore->Write( pS, sl );

				BL += 1 + 1 + sl;
			}
			
			BL++;

			pBlockStore->Seek( 1 );
			pBlockStore->Write( &BL, 2 );
			pBlockStore->SetExt( BL + 3 );
		}
		break;

	case 0x33:
		{
			BYTE n = (BYTE) HardwareFields.size();

			pBlockStore->Write( &n, 1 );

			for ( BYTE i=0; i<n; i++ )
			{
				DWORD h      = HardwareFields[ i ];
				BYTE  HWType = ( h & 0xFF0000 ) >> 16;
				BYTE  HWID   = ( h & 0xFF00   ) >> 8;
				BYTE  HWInfo =   h & 0xFF;

				pBlockStore->Write( &HWType, 1 );
				pBlockStore->Write( &HWID,   1 );
				pBlockStore->Write( &HWInfo, 1 );
			}

			pBlockStore->SetExt( ( n * 3 ) + 2 );
		}
		break;

	case 0x35:
		// TODO
		break;

	case 0x5A:
		{
			BYTE Glue[] = { "XTape!\x1A\x01\x01" };

			pBlockStore->Write( Glue, 9 );
			pBlockStore->SetExt( 10 );
		}
		break;
	}
}

inline void IntToUI( DWORD Ctrl, int v )
{
	WCHAR vv[ 16 ];

	swprintf_s( vv, 15, L"%d", v );

	::SendMessage( GetDlgItem( SubDialog, Ctrl ), WM_SETTEXT, 0, (LPARAM) vv );
}

inline void BlockToWORDValue( DWORD Ctrl, DWORD Offset )
{
	WORD v;

	pBlockStore->Seek( Offset + 1 );

	pBlockStore->Read( &v, 2 );

	IntToUI( Ctrl, (int) v );
}

inline void BlockToBYTEValue( DWORD Ctrl, DWORD Offset )
{
	BYTE v;

	pBlockStore->Seek( Offset + 1 );

	pBlockStore->Read( &v, 1 );

	IntToUI( Ctrl, (int) v );
}

inline void BlockToDWORDValue( DWORD Ctrl, DWORD Offset )
{
	DWORD v;

	pBlockStore->Seek( Offset + 1 );

	pBlockStore->Read( &v, 4 );

	IntToUI( Ctrl, (int) v );
}

inline void BlockToTRIBYTEValue( DWORD Ctrl, DWORD Offset )
{
	DWORD v = 0;

	pBlockStore->Seek( Offset + 1 );

	pBlockStore->Read( &v, 3 );

	IntToUI( Ctrl, (int) v );
}

inline void BlockToCountedString( DWORD Ctrl, DWORD Offset )
{
	pBlockStore->Seek( Offset + 1 );

	BYTE l;

	pBlockStore->Read( &l, 1 );

	BYTE s[ 256 ];

	ZeroMemory( s, 256 );

	pBlockStore->Read( s, l );

	::SendMessage( GetDlgItem( SubDialog, Ctrl ), WM_SETTEXT, 0, (LPARAM) UString( (char *) s ) );
}

const WCHAR *DescribeBlock( BYTE BlockID )
{
	BYTE i = 0 ;

	while ( TZXIDs[ i ] != 0x00 )
	{
		if ( TZXIDs[ i ] == BlockID )
		{
			return TZXBlockTypes[ i ];
		}

		i++;
	}

	return nullptr;
}

void ParseEditingBlock( void )
{
	switch ( SelectedID )
	{
	case 0x10:
		BlockToWORDValue( IDC_PAUSE, 0x00 );

		{
			BYTE Header[ 19 ];
			WORD sz = 0;

			bool SetHeader = false;

			pBlockStore->Seek( 3 );
			pBlockStore->Read( &sz, 2 );

			if ( sz == 19 )
			{
				/* Checksum the block to confirm it's a header */
				pBlockStore->Read( Header, 19 );

				BYTE ck = 0;

				for ( BYTE i=0; i<18; i++ )
				{
					ck ^= Header[ i ];
				}

				if ( ck == Header[ 18 ] )
				{
					/* Valid tape header - populate the fields */
					SetHeader = true;

					::SendMessage( GetDlgItem( SubDialog, IDC_USEHEADER ), BM_SETCHECK, BST_CHECKED, 0 );
					::SendMessage( GetDlgItem( SubDialog, IDC_USEFLAG ), BM_SETCHECK, BST_CHECKED, 0 );
					::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_SETCURSEL, Header[ 1 ], 0 );
					::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_SETCURSEL, 0, 0 );

					WORD DLen   = * (WORD *) &Header[ 12 ];
					WORD Param1 = * (WORD *) &Header[ 14 ];
					WORD Param2 = * (WORD *) &Header[ 16 ];

					WCHAR vv1[ 16 ];
					WCHAR vv2[ 16 ];

					swprintf_s( vv1, 15, L"%d", DLen );
					::SendMessage( GetDlgItem( SubDialog, IDC_DATALEN), WM_SETTEXT, 0, (LPARAM) vv1 );
					
					swprintf_s( vv1, 15, L"%d", Param1 );
					swprintf_s( vv2, 15, L"%d", Param2 );

					switch ( Header[ 1 ] )
					{
					case 0:
						::SendMessage( GetDlgItem( SubDialog, IDC_STARTLINE ), WM_SETTEXT, 0, (LPARAM) vv1 );
						::SendMessage( GetDlgItem( SubDialog, IDC_VARADDR ), WM_SETTEXT, 0, (LPARAM) vv2 );
						break;
					case 1:
					case 2:
						{
							WCHAR vv3[] = { 0, 0 };
							vv3[0] = Header[ 15 ];

							::SendMessage( GetDlgItem( SubDialog, IDC_VARIABLE ), WM_SETTEXT, 0, (LPARAM) vv3 );
						}
						break;
					case 3:
						::SendMessage( GetDlgItem( SubDialog, IDC_LOADADDR ), WM_SETTEXT, 0, (LPARAM) vv1 );
						break;
					}

					/* Filename - it's padding with 0x20 */
					BYTE fname[11];

					fname[ 10 ] = 0;

					memcpy( fname, &Header[ 2 ], 10 );

					for ( BYTE n=9; n>0; n-- )
					{
						if ( fname[ n ] == 0x20 ) { fname[ n ] = 0; } else { break; }
					}

					::SendMessage( GetDlgItem( SubDialog, IDC_FILENAME ), WM_SETTEXT, 0, (LPARAM) UString( (char *) fname ) );
				}
			}
			else
			{
				/* Checksum the data part to see if it looks like a flag+sum stream */
				BYTE ck = 0;

				for ( WORD i=0; i<sz; i++ )
				{
					BYTE b = 0;

					pBlockStore->Read( &b, 1 );

					ck ^= b;
				}

				if ( ck == 0 )
				{
					::SendMessage( GetDlgItem( SubDialog, IDC_USEFLAG ), BM_SETCHECK, BST_CHECKED, 0 );
				}
			}

			if ( SetHeader )
			{
				::SendMessage( GetDlgItem( SubDialog, IDC_HEADERBYTE ), BM_SETCHECK, BST_CHECKED, 0 );
			}
			else
			{
				::SendMessage( GetDlgItem( SubDialog, IDC_DATABYTE ), BM_SETCHECK, BST_CHECKED, 0 );
			}
		}

		break;

	case 0x11:
		BlockToWORDValue( IDC_PILOTL,   0x00 );
		BlockToWORDValue( IDC_SYNC1L,   0x02 );
		BlockToWORDValue( IDC_SYNC2L,   0x04 );
		BlockToWORDValue( IDC_ZEROL,    0x06 );
		BlockToWORDValue( IDC_ONEL,     0x08 );
		BlockToWORDValue( IDC_PILOTC,   0x0A );
		BlockToBYTEValue( IDC_USEDBITS, 0x0C );
		BlockToWORDValue( IDC_PAUSE,    0x0D );
		break;

	case 0x12:
		BlockToWORDValue( IDC_1NTEXT, 0x00 );
		BlockToWORDValue( IDC_1STEXT, 0x02 );
		break;

	case 0x13:
		{
			Pulses.clear();

			BYTE PulseCount;
			WORD Length;

			pBlockStore->Seek( 1 );
			pBlockStore->Read( &PulseCount, 1 );

			for ( BYTE i=0; i<PulseCount; i++ )
			{
				pBlockStore->Seek( 2 + ( 2 * i ) );
				pBlockStore->Read( &Length, 2 );

				Pulses.push_back( Length );

				WCHAR vv[ 64 ];

				swprintf_s( vv, 63, L"%d T states", Length );

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
			}
		}
		break;

	case 0x14:
		BlockToWORDValue( IDC_ZEROL,    0x00 );
		BlockToWORDValue( IDC_ONEL,     0x02 );
		BlockToBYTEValue( IDC_USEDBITS, 0x04 );
		BlockToWORDValue( IDC_PAUSE,    0x05 );
		break;

	case 0x20:
	case 0x24:
		BlockToWORDValue( IDC_1NTEXT,   0x00 );
		break;

	case 0x21:
	case 0x30:
		BlockToCountedString( IDC_1STEXT, 0x00 );
		break;

	case 0x23:
		{
			unsigned short Jump;

			pBlockStore->Seek( 1 );
			pBlockStore->Read( &Jump, 2 );

			JumpVectors.clear();
			JumpVectors.push_back( SelectedBlk + Jump );

			WORD Target = (WORD) SelectedBlk; Target += Jump;

			if ( Target < pFiles->size() )
			{
				WCHAR vv[ 300 ];

				swprintf_s( vv, 299, L"%d: %s", Target, DescribeBlock( (*pFiles)[ Target ].Attributes[14] ) );

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
			}
			else
			{
				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) L"Corrupt block" );
			}
		}
		break;

	case 0x26:
		{
			JumpVectors.clear();

			BYTE JumpCount;

			unsigned short Jump;

			pBlockStore->Seek( 1 );
			pBlockStore->Read( &JumpCount, 1 );

			for ( BYTE i=0; i<JumpCount; i++ )
			{
				pBlockStore->Seek( 2 + ( 2 * i ) );
				pBlockStore->Read( &Jump, 2 );

				JumpVectors.push_back( SelectedBlk + Jump );

				WORD Target = (WORD) SelectedBlk; Target += Jump;

				if ( Target < pFiles->size() )
				{
					WCHAR vv[ 300 ];

					swprintf_s( vv, 299, L"Call to block: %d %s", Target, DescribeBlock( (*pFiles)[ Target ].Attributes[14] ) );

					::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
				}
				else
				{
					::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) L"Corrupt block" );
				}
			}
		}
		break;

	case 0x28:
		{
			SelectFields.clear();

			BYTE JumpCount;

			unsigned short Jump;

			pBlockStore->Seek( 3 );
			pBlockStore->Read( &JumpCount, 1 );

			for ( BYTE i=0; i<JumpCount; i++ )
			{
				BYTE StrL;
				BYTE StrC[ 256 ];

				pBlockStore->Read( &Jump, 2 );

				WORD Target = (WORD) SelectedBlk; Target += Jump;

				ZeroMemory( StrC, 256 );

				pBlockStore->Read( &StrL, 1 );
				pBlockStore->Read( StrC, StrL );

				std::wstring SelectTitle = UString( (char *) StrC );

				if ( Target < pFiles->size() )
				{
					SelectFields.push_back( std::make_pair<WORD, std::wstring>( Target, SelectTitle ) );

					WCHAR vv[ 300 ];

					swprintf_s( vv, 299, L"%s: %d %s", SelectTitle.c_str(),Target, DescribeBlock( (*pFiles)[ Target ].Attributes[14] ) );

					::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
				}
				else
				{
					::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) L"Corrupt block" );
				}
			}
		}
		break;

	case 0x2B:
		{
			BYTE sig;

			pBlockStore->Seek( 5 );
			pBlockStore->Read( &sig, 1 );

			if ( sig == 0x00 )
			{
				::SendMessage( GetDlgItem( SubDialog, IDC_LOWRADIO ), BM_SETCHECK, BST_CHECKED, 0 );
				::SendMessage( GetDlgItem( SubDialog, IDC_HIGHRADIO ), BM_SETCHECK, BST_UNCHECKED, 0 );
			}
			else
			{
				::SendMessage( GetDlgItem( SubDialog, IDC_HIGHRADIO ), BM_SETCHECK, BST_CHECKED, 0 );
				::SendMessage( GetDlgItem( SubDialog, IDC_LOWRADIO ), BM_SETCHECK, BST_UNCHECKED, 0 );
			}
		}
		break;

	case 0x31:
		BlockToBYTEValue( IDC_1NTEXT,     0x00 );
		BlockToCountedString( IDC_1STEXT, 0x01 );
		break;

	case 0x32:
		{
			ArchiveFields.clear();

			BYTE FieldCount;

			pBlockStore->Seek( 3 );
			pBlockStore->Read( &FieldCount, 1 );

			for ( BYTE i=0; i<FieldCount; i++ )
			{
				BYTE FType;
				BYTE StrL;
				BYTE StrC[ 256 ];

				ZeroMemory( StrC, 256 );

				pBlockStore->Read( &FType, 1 );
				pBlockStore->Read( &StrL, 1 );
				pBlockStore->Read( StrC, StrL );

				std::wstring FieldString = UString( (char *) StrC );

				ArchiveFields.push_back( std::make_pair<BYTE, std::wstring>( FType, FieldString ) );

				WCHAR *pType = L"Unknown";
				
				if ( FType <= 8    ) { pType = (WCHAR *) TZXArchiveFieldTypes[ FType ]; }
				if ( FType == 0xFF ) { pType = L"Comment(s)"; }

				WCHAR vv[ 300 ];

				swprintf_s( vv, 299, L"%s: %s", pType, FieldString.c_str() );

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
			}
		}
		break;

	case 0x33:
		{
			HardwareFields.clear();

			BYTE FieldCount;

			pBlockStore->Seek( 1 );
			pBlockStore->Read( &FieldCount, 1 );

			for ( BYTE i=0; i<FieldCount; i++ )
			{
				DWORD HType = 0;
				DWORD HID   = 0;
				DWORD HInfo = 0;

				pBlockStore->Read( &HType, 1 );
				pBlockStore->Read( &HID,   1 );
				pBlockStore->Read( &HInfo, 1 );

				DWORD HDW = ( HType << 16 ) | ( HID << 8 ) | HInfo;

				HardwareFields.push_back( HDW );

				WCHAR vv[ 300 ];

				swprintf_s( vv, 299, L"%s: %s", GetHardwareString( HType, HID ), TZXHInfos[ HInfo & 3 ] );

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
			}
		}
		break;
	}
}

const WCHAR *PromptBlock( BYTE BlockID )
{
	BYTE i = 0 ;

	while ( TZXIDs[ i ] != 0x00 )
	{
		if ( TZXIDs[ i ] == BlockID )
		{
			return BlockPrompt[ i ];
		}

		i++;
	}

	return nullptr;
}

INT_PTR CALLBACK SelectDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_INITDIALOG:
		{
			for ( BYTE i=0; i<255; i++ )
			{
				if ( TZXBlockTypes[i] == nullptr )
				{
					break;
				}

				::SendMessage( GetDlgItem( hDlg, IDC_TYPELIST ), LB_ADDSTRING, 0, (LPARAM) TZXBlockTypes[ i ] );
			}
		}
		break;

	case WM_COMMAND:
		DWORD id = LOWORD( wParam );

		if ( id == IDCANCEL )
		{
			SelectedID = 0xFF;

			EndDialog( hDlg, 0 );
		}

		if ( id == IDOK )
		{
			EndDialog( hDlg, 0 );
		}

		if ( id == IDC_TYPELIST )
		{
			if ( HIWORD( wParam ) == LBN_SELCHANGE )
			{
				::EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );

				WORD Index = (WORD) ::SendMessage( GetDlgItem( hDlg, IDC_TYPELIST ), LB_GETCURSEL, 0, 0 );

				SelectedID = TZXIDs[ Index ];
			}
		}
		break;
	}

	return FALSE;
}

INT_PTR CALLBACK InsertDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_INITDIALOG:
		{
			DialogWnd = hDlg;

			if ( pFiles != nullptr )
			{
				NativeFileIterator iFile;

				for ( iFile = pFiles->begin(); iFile != pFiles->end(); iFile++ )
				{
					BYTE BlockID = iFile->Attributes[ 14 ];

					WCHAR File[ 256 ];

					if ( BlockID == 0 )
					{
						BYTE BFile[ 256 ];

						/* This has been resolved to a file */
						if ( iFile->Flags & FF_Extension )
						{
							rsprintf( BFile, "File: %s.%s", iFile->Filename, iFile->Extension );
						}
						else
						{
							rsprintf( BFile, "File: %s", iFile->Filename );
						}

						wcscpy_s( File, 255, UString( (char *) BFile ) );
					}
					else
					{
						swprintf_s( File, 256, L"%03d: %s", iFile->fileID, DescribeBlock( BlockID ) );
					}

					::SendMessage( GetDlgItem( hDlg, IDC_BLOCKLIST ), LB_ADDSTRING, 0, (LPARAM) File );
				}
			}

			::SendMessage( GetDlgItem( hDlg, IDC_BLOCKLIST ), LB_ADDSTRING, 0, (LPARAM) L"<Insert after all blocks>" );
		}
		break;

	case WM_COMMAND:
		DWORD id = LOWORD( wParam );

		if ( id == IDCANCEL )
		{
			SelectedID = 0xFF;

			EndDialog( hDlg, 0 );
		}

		if ( id == IDOK )
		{
			EndDialog( hDlg, 0 );
		}

		if ( id == IDC_BLOCKLIST )
		{
			if ( HIWORD( wParam ) == LBN_SELCHANGE )
			{
				::EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );

				WORD Index = (WORD) ::SendMessage( GetDlgItem( hDlg, IDC_BLOCKLIST ), LB_GETCURSEL, 0, 0 );

				SelectedBlk = Index;
			}
		}
		break;
	}

	return FALSE;
}

void UpdatePercent( DWORD Pc, DWORD Tc, DWORD DT )
{
	if ( SubDialog == NULL )
	{
		return;
	}

	WCHAR num[ 64 ];

	::SendMessage( GetDlgItem( SubDialog, Tc ), WM_GETTEXT, 63, (LPARAM) num );

	double v = (double) _wtoi( num );
	double m = (double) DT;

	double p = floor( (m / v ) * 100.0 );

	swprintf_s( num, 64, L"%d%%", (int) p );

	::SendMessage( GetDlgItem( SubDialog, Pc ), WM_SETTEXT, 0, (LPARAM) num );
}

void DoPickerMenu( DWORD TC, const MenuOption *opts )
{
	BYTE i = 0;

	HMENU hMenu    = CreatePopupMenu();

	while ( opts[i].Name[ 0 ] != 0 )
	{
		AppendMenu( hMenu, MF_STRING, WMBASE + i, opts[i].Name );

		i++;
	}

	MenuPickControl = TC;
	MenuPickSelect  = (MenuOption *) opts;

	RECT rect;

	GetWindowRect( GetDlgItem( SubDialog, TC ), &rect );

	TrackPopupMenu( hMenu, 0, rect.left, rect.bottom, 0, SubDialog, NULL);
}

void CheckListButtons()
{
	int i = ::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_GETCOUNT, 0, 0 );

	if ( i == 0 )
	{
		EnableWindow( GetDlgItem( SubDialog, IDC_REMOVEITEM ), FALSE );
		EnableWindow( GetDlgItem( SubDialog, IDC_UPITEM ),     FALSE );
		EnableWindow( GetDlgItem( SubDialog, IDC_DOWNITEM ),   FALSE );

		return;
	}

	int s = ::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_GETCURSEL,   0, 0 );

	if ( i == 1 )
	{
		EnableWindow( GetDlgItem( SubDialog, IDC_UPITEM ),     FALSE );
		EnableWindow( GetDlgItem( SubDialog, IDC_DOWNITEM ),   FALSE );

		if ( s == LB_ERR )
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_REMOVEITEM ), FALSE );
		}
		else
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_REMOVEITEM ), TRUE );
		}

		return;
	}

	if ( s == LB_ERR )
	{
		EnableWindow( GetDlgItem( SubDialog, IDC_UPITEM ),     FALSE );
		EnableWindow( GetDlgItem( SubDialog, IDC_DOWNITEM ),   FALSE );
		EnableWindow( GetDlgItem( SubDialog, IDC_REMOVEITEM ), FALSE );
	}
	else
	{
		EnableWindow( GetDlgItem( SubDialog, IDC_REMOVEITEM ), TRUE );

		if ( s > 0 )
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_UPITEM ), TRUE );
		}
		else
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_UPITEM ), FALSE );
		}

		if ( s < ( i - 1 ) )
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_DOWNITEM ), TRUE );
		}
		else
		{
			EnableWindow( GetDlgItem( SubDialog, IDC_DOWNITEM ), FALSE );
		}
	}
}

INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static const MenuOption PilotPulseLengths[] = {
		{ L"ZX Spectrum ROM Pilot: 2168", 2168 },
		{ NULL, NULL }
	};

	static const MenuOption PilotCounts[] = {
		{ L"ZX Spectrum ROM Header: 8063", 8063 },
		{ L"ZX Spectrum ROM Data: 3223", 3223 },
		{ NULL, NULL }
	};

	static const MenuOption Sync1PulseLengths[] = {
		{ L"ZX Spectrum ROM Sync 1: 667", 667 },
		{ NULL, NULL }
	};

	static const MenuOption Sync2PulseLengths[] = {
		{ L"ZX Spectrum ROM Sync 2: 735", 735 },
		{ NULL, NULL }
	};

	static const MenuOption ZeroPulseLengths[] = {
		{ L"ZX Spectrum ROM Zero: 855", 855 },
		{ L"Speedlock 1 Zero: 585", 585 },
		{ L"Speedlock 2 Zero: 725", 725 },
		{ NULL, NULL }
	};

	static const MenuOption OnePulseLengths[] = {
		{ L"ZX Spectrum ROM One: 1710", 1710 },
		{ L"Speedlock 1 One: 1170", 1170 },
		{ L"Speedlock 2 One: 1449", 1449 },
		{ NULL, NULL }
	};

	switch (message )
	{
	case WM_INITDIALOG:
		{
			if ( TopLevelCreated )
			{
				return FALSE;
			}

			DialogWnd = hDlg;

			TopLevelCreated = true;

			Pulses.clear();
			ArchiveFields.clear();
			HardwareFields.clear();
			JumpVectors.clear();
			SelectFields.clear();

			if ( ( SelectedID == 0x26 ) || ( SelectedID == 0x28 ) || ( SelectedID == 0x32 ) || ( SelectedID == 0x33 ) || ( SelectedID == 0x13 ) )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_LISTCONFIG ), hDlg, ConfigDialogProc, 0 );
			}

			if ( ( SelectedID == 0x12 ) || ( SelectedID == 0x21 ) || ( SelectedID == 0x24 ) || ( SelectedID == 0x20 ) || ( SelectedID == 0x30 ) || ( SelectedID == 0x31 ) )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_1NCONFIG ), hDlg, ConfigDialogProc, 0 );

				switch ( SelectedID )
				{
				case 0x12:
					::SendMessage( GetDlgItem( SubDialog, IDC_1NPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Pulse length (Ts):" );
					::SendMessage( GetDlgItem( SubDialog, IDC_1SPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Pulse count:" );
					break;
				case 0x20:
					::SendMessage( GetDlgItem( SubDialog, IDC_1NPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Pause length (ms):" );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1SPROMPT ), SW_HIDE );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1STEXT ), SW_HIDE );
					break;
				case 0x21:
					::SendMessage( GetDlgItem( SubDialog, IDC_1SPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Group name:" );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1NPROMPT ), SW_HIDE );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1NTEXT ), SW_HIDE );
					break;
				case 0x24:
					::SendMessage( GetDlgItem( SubDialog, IDC_1NPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Repetitions:" );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1SPROMPT ), SW_HIDE );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1STEXT ), SW_HIDE );
					break;
				case 0x30:
					::SendMessage( GetDlgItem( SubDialog, IDC_1SPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Text Description:" );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1NPROMPT ), SW_HIDE );
					::ShowWindow( GetDlgItem( SubDialog, IDC_1NTEXT ), SW_HIDE );
					break;

				case 0x31:
					::SendMessage( GetDlgItem( SubDialog, IDC_1SPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Pause (s):" );
					::SendMessage( GetDlgItem( SubDialog, IDC_1SPROMPT ), WM_SETTEXT, 0, (LPARAM) L"Message:" );
					break;
				}
			}

			if ( ( SelectedID == 0x11 ) || ( SelectedID == 0x14 ) )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_DATACONFIG ), hDlg, ConfigDialogProc, 0 );

				if ( SelectedID == 0x14 )
				{
					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTL ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTC ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC1L ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC2L ), SW_HIDE );

					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTLT ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTCT ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC1LT ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC2LT ), SW_HIDE );

					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTLCHOOSE ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTCCHOOSE ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC1CHOOSE  ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC2CHOOSE  ), SW_HIDE );

					ShowWindow( GetDlgItem( SubDialog, IDC_PILOTP ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC1P ), SW_HIDE );
					ShowWindow( GetDlgItem( SubDialog, IDC_SYNC2P ), SW_HIDE );
				}

				::SendMessage( GetDlgItem( SubDialog, IDC_PILOTL ), WM_SETTEXT, 0, (LPARAM) L"2168" );
				::SendMessage( GetDlgItem( SubDialog, IDC_PILOTC ), WM_SETTEXT, 0, (LPARAM) L"8063" );
				::SendMessage( GetDlgItem( SubDialog, IDC_SYNC1L ), WM_SETTEXT, 0, (LPARAM) L"667"  );
				::SendMessage( GetDlgItem( SubDialog, IDC_SYNC2L ), WM_SETTEXT, 0, (LPARAM) L"735"  );
				::SendMessage( GetDlgItem( SubDialog, IDC_ZEROL  ), WM_SETTEXT, 0, (LPARAM) L"855"  );
				::SendMessage( GetDlgItem( SubDialog, IDC_ONEL   ), WM_SETTEXT, 0, (LPARAM) L"1710" );

				::SendMessage( GetDlgItem( SubDialog, IDC_USEDBITS ), WM_SETTEXT, 0, (LPARAM) L"0" );
				::SendMessage( GetDlgItem( SubDialog, IDC_PAUSE    ), WM_SETTEXT, 0, (LPARAM) L"1" );

				::SendMessage( GetDlgItem( SubDialog, IDC_DATASOURCE), WM_SETTEXT, 0, (LPARAM) L"No file chosen" );
			}

			if ( SelectedID == 0x10 )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_STDCONFIG ), hDlg, ConfigDialogProc, 0 );

				::SendMessage( GetDlgItem( SubDialog, IDC_DATASOURCE), WM_SETTEXT, 0, (LPARAM) L"No file chosen" );

				::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_ADDSTRING, 0, (LPARAM) L"Program" );
				::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_ADDSTRING, 0, (LPARAM) L"Number Array" );
				::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_ADDSTRING, 0, (LPARAM) L"Character Array" );
				::SendMessage( GetDlgItem( SubDialog, IDC_STDFILETYPE ), CB_ADDSTRING, 0, (LPARAM) L"Bytes" );

				/* Initialise the fields */
				if ( IsNewBlock )
				{
					::SendMessage( GetDlgItem( SubDialog, IDC_HEADERBYTE ), BM_SETCHECK, BST_CHECKED, 0 );
					::SendMessage( GetDlgItem( SubDialog, IDC_USEFLAG ), BM_SETCHECK, BST_CHECKED, 0 );
					::SendMessage( GetDlgItem( SubDialog, IDC_PAUSE ), WM_SETTEXT, 0, (LPARAM) L"1000" );

					::SendMessage( GetDlgItem( SubDialog, IDC_LOADADDR ),  WM_SETTEXT, 0, (LPARAM) L"4000" );
					::SendMessage( GetDlgItem( SubDialog, IDC_VARADDR ),   WM_SETTEXT, 0, (LPARAM) L"4800" );
					::SendMessage( GetDlgItem( SubDialog, IDC_VARIABLE ),  WM_SETTEXT, 0, (LPARAM) L"a" );
					::SendMessage( GetDlgItem( SubDialog, IDC_STARTLINE ), WM_SETTEXT, 0, (LPARAM) L"10" );
				}
			}

			if ( SelectedID == 0x23 )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_JUMPCONFIG ), hDlg, ConfigDialogProc, 0 );
			}

			if ( SelectedID == 0x2B )
			{
				SubDialog = CreateDialogParam( hInstance, MAKEINTRESOURCE( IDD_PICKCONFIG ), hDlg, ConfigDialogProc, 0 );

				::SendMessage( GetDlgItem( SubDialog, IDC_HIGHRADIO ), BM_SETCHECK, BST_CHECKED, 0 );
				::SendMessage( GetDlgItem( SubDialog, IDC_LOWRADIO ), BM_SETCHECK, BST_UNCHECKED, 0 );
			}

			if ( SubDialog != NULL )
			{
				RECT r;

				GetClientRect( hDlg, &r );

				ShowWindow( SubDialog, SW_SHOW );

				SetWindowPos( SubDialog, NULL, 0, 80, r.right - r.left, r.bottom - r.top - 32 - 84, SWP_NOREPOSITION | SWP_NOZORDER );
			}

			SendMessage( GetDlgItem( hDlg, IDC_PROMPT ), WM_SETTEXT, 0, (LPARAM) PromptBlock( SelectedID ) );
			
			WCHAR title[ 256 ];

			swprintf_s( title, 256, L"Configure '%s' block", DescribeBlock( SelectedID ) );

			SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) title );

			if ( !IsNewBlock )
			{
				ParseEditingBlock();
			}

			if ( ( SelectedID == 0x26 ) || ( SelectedID == 0x28 ) || ( SelectedID == 0x32 ) || ( SelectedID == 0x33 ) || ( SelectedID == 0x13 ) )
			{
				CheckListButtons();
			}
		}

		break;

	case WM_COMMAND:
		DWORD id = LOWORD( wParam );

		switch (id)
		{
		case IDCANCEL:
			{
				SelectedID = 0xFF;

				EndDialog( hDlg, 0 );
			}
			break;

		case IDOK:
			{
				if ( ( SelectedID == 0x11 ) || ( SelectedID == 0x14 ) )
				{
					if ( ( IsNewBlock ) && ( !ChangedDataSet ) )
					{
						MessageBox( hDlg, L"Creating a new data block requires specifying a data source", L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

						break;
					}
				}

				if ( SelectedID == 0x10 )
				{
					bool UseHeader = false;

					if ( ::SendMessage( GetDlgItem( SubDialog, IDC_USEHEADER ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { UseHeader = true; }

					if ( ( !UseHeader ) && ( !ChangedDataSet ) )
					{
						MessageBox( hDlg, L"Creating a new data block without specifing 'Use Header' requires specifying a data source", L"NUTS TZX Configuration", MB_ICONEXCLAMATION | MB_OK );

						break;
					}
				}

				WriteEditedBlock();

				EndDialog( hDlg, 0 );
			}
			break;

		case IDC_PILOTL:
			UpdatePercent( IDC_PILOTP, IDC_PILOTL, 2168 );
			break;

		case IDC_SYNC1L:
			UpdatePercent( IDC_SYNC1P, IDC_SYNC1L, 667 );
			break;

		case IDC_SYNC2L:
			UpdatePercent( IDC_SYNC2P, IDC_SYNC2L, 735 );
			break;

		case IDC_ZEROL:
			UpdatePercent( IDC_ZEROP, IDC_ZEROL, 855 );
			break;

		case IDC_ONEL:
			UpdatePercent( IDC_ONEP, IDC_ONEL, 1710 );
			break;

		case IDC_PILOTLCHOOSE:
			DoPickerMenu( IDC_PILOTL, PilotPulseLengths );
			break;

		case IDC_PILOTCCHOOSE:
			DoPickerMenu( IDC_PILOTC, PilotCounts );
			break;

		case IDC_SYNC1CHOOSE:
			DoPickerMenu( IDC_SYNC1L, Sync1PulseLengths );
			break;

		case IDC_SYNC2CHOOSE:
			DoPickerMenu( IDC_SYNC2L, Sync2PulseLengths );
			break;

		case IDC_ZEROCHOOSE:
			DoPickerMenu( IDC_ZEROL, ZeroPulseLengths );
			break;

		case IDC_ONECHOOSE:
			DoPickerMenu( IDC_ONEL, OnePulseLengths );
			break;

		case IDC_RESETDATA:
			ChangedDataSet = false;

			::SendMessage( GetDlgItem( SubDialog, IDC_DATASOURCE), WM_SETTEXT, 0, (LPARAM) L"No file chosen" );

			break;

		case IDC_DATASEL:
			{
				std::wstring fname;

				bool r = OpenAnyFileDialog( hDlg, fname, L"Select data source" );

				if ( r )
				{
					DataSetPath    = fname;
					ChangedDataSet = true;

					::SendMessage( GetDlgItem( SubDialog, IDC_DATASOURCE), WM_SETTEXT, 0, (LPARAM) fname.c_str() );
				}
			}
			break;

		case IDC_JUMPSELECT:
			{
				int b = PickABlock( );

				if ( b != -1 )
				{
					JumpVectors.clear();
					JumpVectors.push_back( b );

					WCHAR vv[ 300 ];

					swprintf_s( vv, 299, L"%d: %s", b, DescribeBlock( (*pFiles)[ b ].Attributes[ 14 ] ) );

					::SendMessage( GetDlgItem( SubDialog, IDC_JUMPTARGET ), WM_SETTEXT, 0, (LPARAM) vv );
				}
			}

		case IDC_ADDITEM:
			{
				switch (SelectedID)
				{
				case 0x13:
					{
						int b = PickAString( L"Enter a pulse length:" );

						if ( b != -1 )
						{
							DWORD n = (DWORD) _wtoi( StringEntryResponse.c_str() );

							Pulses.push_back( n );

							WCHAR vv[ 64 ];

							swprintf_s( vv, 63, L"%d T states", n );

							::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
						}
					}
					break;

				case 0x26:
					{
						int b = PickABlock();

						if ( b != - 1 )
						{
							JumpVectors.push_back( b );

							WCHAR vv[ 300 ];

							swprintf_s( vv, 299, L"Call to block: %d %s", b, DescribeBlock( (*pFiles)[b].Attributes[14] ) );

							::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
						}
					}
					break;

				case 0x28:
					{
						int b = PickABlock();

						if ( b != -1 )
						{
							int r = PickAString( L"Enter a name for the select entry:" );

							if ( r != -1 )
							{
								SelectFields.push_back( std::make_pair<WORD, std::wstring>( b, StringEntryResponse ) );

								WCHAR vv[ 300 ];

								swprintf_s( vv, 299, L"%s: %d %s", StringEntryResponse.c_str(), b, DescribeBlock( (*pFiles)[b].Attributes[14] ) );

								::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
							}
						}
					}
					break;

				case 0x33:
					{
						int h = PickHardware();

						if ( h != -1 )
						{
							DWORD HDW = (DWORD) h;

							HardwareFields.push_back( HDW );

							WCHAR vv[ 300 ];

							swprintf_s( vv, 299, L"%s: %s", GetHardwareString( (HDW & 0xFF0000)>>16,(HDW & 0xFF00)>>8 ), TZXHInfos[ HDW & 3 ] );

							::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
						}
					}
					break;

				case 0x32:
					{
						int r = PickArchive();

						if ( r != -1 )
						{
							ArchiveFields.push_back( std::make_pair<BYTE, std::wstring>( ArchiveField, ArchiveString ) );

							WCHAR *pType = L"Unknown";
				
							if ( ArchiveField <= 8    ) { pType = (WCHAR *) TZXArchiveFieldTypes[ ArchiveField ]; }
							if ( ArchiveField == 0xFF ) { pType = L"Comment(s)"; }

							WCHAR vv[ 300 ];

							swprintf_s( vv, 299, L"%s: %s", pType, ArchiveString.c_str() );

							::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_ADDSTRING, 0, (LPARAM) vv );
						}
					}
					break;
				}

				CheckListButtons();
			}
			break;

		case IDC_REMOVEITEM:
			{
				/* Get the item */
				int i = ::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_GETCURSEL, 0, 0 );

				/* Delete it from the list box */
				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_DELETESTRING, i, 0 );

				switch ( SelectedID )
				{
				case 0x13:
					/* Sigh. Yes, this really is the recommended way. */
					Pulses.erase( Pulses.begin() + i );
					break;
				case 0x26:
					JumpVectors.erase( JumpVectors.begin() + i );
					break;

				case 0x28:
					SelectFields.erase( SelectFields.begin() + i );
					break;

				case 0x32:
					ArchiveFields.erase( ArchiveFields.begin() + i );
					break;

				case 0x33:
					HardwareFields.erase( HardwareFields.begin() + i );
					break;
				}

				CheckListButtons();
			}
			break;

		case IDC_UPITEM:
		case IDC_DOWNITEM:
			{
				/* Get the item */
				int i = ::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_GETCURSEL, 0, 0 );

				/* Get the text from the list box, then delete it from its current slot */
				WCHAR vv[ 256 ];

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_GETTEXT, i, (LPARAM) vv );

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_DELETESTRING, i, 0 );

				/* Re-insert it at the new position */
				int NewPos = i + 1;

				if ( id == IDC_UPITEM )
				{
					NewPos = i - 1;
				}

				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_INSERTSTRING, NewPos, (LPARAM) vv );
				::SendMessage( GetDlgItem( SubDialog, IDC_ITEMLIST ), LB_SETCURSEL, NewPos, 0 );

				switch ( SelectedID )
				{
				case 0x13:
					/* Sigh. Yes, this really is the recommended way. */
					iter_swap( Pulses.begin() + i, Pulses.begin() + NewPos );
					break;
				case 0x26:
					iter_swap( JumpVectors.begin() + i, JumpVectors.begin() + NewPos );
					break;

				case 0x28:
					iter_swap( SelectFields.begin() + i, SelectFields.begin() + NewPos );
					break;

				case 0x32:
					iter_swap( ArchiveFields.begin() + i, ArchiveFields.begin() + NewPos );
					break;

				case 0x33:
					iter_swap( HardwareFields.begin() + i,  HardwareFields.begin() + NewPos );
					break;
				}

				CheckListButtons();
			}
			break;

		case IDC_ITEMLIST:
			{
				if ( HIWORD( wParam ) == LBN_SELCHANGE )
				{
					CheckListButtons();
				}
			}
			break;
		}

		if ( ( id >= WMBASE ) && ( id <= (WMBASE + 16 ) ) )
		{
			WCHAR num[ 16 ];

			swprintf_s( num, 16, L"%d", MenuPickSelect[ id - WMBASE ].Value );

			::SendMessage( GetDlgItem( SubDialog, MenuPickControl ), WM_SETTEXT, 0, (LPARAM) num );
		}
	}

	return FALSE;
}

TZXCONFIG_API BYTE TZXSelectNewBlock( HWND hParent )
{
	SelectedID = 0xFF;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_SELECTBLOCK ), hParent, SelectDialogProc, NULL );

	return SelectedID;
}

TZXCONFIG_API int TZXConfigureBlock( HWND hParent, WORD BlockNum, BYTE BlockID, CTempFile *pStore, bool NewBlock, bool AllowReplaceData, std::vector<NativeFile> &Files )
{
	pFiles = &Files;

	SelectedID  = BlockID;
	IsNewBlock  = NewBlock;
	SelectedBlk = BlockNum;

	pBlockStore = pStore;

	ChangedDataSet = false;
	DataSetPath    = L"";

	TopLevelCreated = false;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_CONFIGBLOCK ), hParent, ConfigDialogProc, NULL );

	if ( SelectedID == 0xFF )
	{
		return -1;
	}

	return 0;
}

TZXCONFIG_API int TZXSelectBlock( HWND hParent, std::vector<NativeFile> &Files )
{
	pFiles = &Files;

	SelectedBlk = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_INSERT ), hParent, InsertDialogProc, NULL );

	return SelectedBlk;
}

INT_PTR CALLBACK PickABlockDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_INITDIALOG:
		{
			if ( pFiles != nullptr )
			{
				NativeFileIterator iFile;

				for ( iFile = pFiles->begin(); iFile != pFiles->end(); iFile++ )
				{
					BYTE BlockID = iFile->Attributes[ 14 ];

					WCHAR File[ 256 ];

					if ( BlockID == 0 )
					{
						BYTE BFile[ 256 ];

						/* This has been resolved to a file */
						if ( iFile->Flags & FF_Extension )
						{
							rsprintf( BFile, "File: %s.%s", iFile->Filename, iFile->Extension );
						}
						else
						{
							rsprintf( BFile, "File: %s", iFile->Filename );
						}

						wcscpy_s( File, 255, UString( (char *) BFile ) );
					}
					else
					{
						swprintf_s( File, 256, L"%03d: %s", iFile->fileID, DescribeBlock( BlockID ) );
					}

					::SendMessage( GetDlgItem( hDlg, IDC_BLOCKLIST ), LB_ADDSTRING, 0, (LPARAM) File );
				}
			}
		}
		break;

	case WM_COMMAND:
		DWORD id = LOWORD( wParam );

		if ( id == IDCANCEL )
		{
			SelectedID = 0xFF;

			EndDialog( hDlg, 0 );
		}

		if ( id == IDOK )
		{
			EndDialog( hDlg, 0 );
		}

		if ( id == IDC_BLOCKLIST )
		{
			if ( HIWORD( wParam ) == LBN_SELCHANGE )
			{
				::EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );

				WORD Index = (WORD) ::SendMessage( GetDlgItem( hDlg, IDC_BLOCKLIST ), LB_GETCURSEL, 0, 0 );

				SelectedBlk = Index;
			}
		}
		break;
	}

	return FALSE;
}

int PickABlock( void )
{
	SelectedBlk = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_PICKABLOCK ), DialogWnd, PickABlockDialogProc, NULL );

	return SelectedBlk;
}


INT_PTR CALLBACK StringEntryDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		if ( SelectedID == 0x13 )
		{
			LONG Style = GetWindowLong( GetDlgItem( hDlg, IDC_STRINGENTRY ), GWL_STYLE );

			Style |= ES_NUMBER;

			SetWindowLong( GetDlgItem( hDlg, IDC_STRINGENTRY ), GWL_STYLE, Style );
		}

		::SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) StringEntryTitle.c_str() );

		::SetFocus( GetDlgItem( hDlg, IDC_STRINGENTRY ) );

		break;

	case WM_COMMAND:
		{
			DWORD id = LOWORD( wParam );

			if ( id == IDCANCEL )
			{
				StringEntryResult = -1;

				EndDialog( hDlg, 0 );
			}

			if ( id == IDOK )
			{
				WCHAR vv[ 256 ];

				::SendMessage( GetDlgItem( hDlg, IDC_STRINGENTRY ), WM_GETTEXT, 255, (LPARAM) vv );

				StringEntryResult = 0;

				StringEntryResponse = vv;

				EndDialog( hDlg, 0 );
			}
		}
		break;
	}

	return FALSE;
}

int PickAString( std::wstring Prompt )
{
	StringEntryTitle  = Prompt;
	StringEntryResult = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_STRINGBOX ), DialogWnd, StringEntryDialogProc, NULL );

	return StringEntryResult;
}


INT_PTR CALLBACK HardwareDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			WCHAR *t[] = {
				L"Computers", L"Storage", L"Memory", L"Sound", L"Joysticks", L"Mice", L"Other Controllers", L"Serial Ports",
				L"Parallel Ports", L"Printers", L"Modems", L"Digitizers", L"Network adapters", L"Keyboards & Keypads", L"AD/DA Converters",
				L"EPROM Programmers", L"Graphics"
			};

			for (BYTE i=0; i<0x10; i++)
			{
				::SendMessage( GetDlgItem( hDlg, IDC_HWID ), CB_ADDSTRING, 0, (LPARAM) t[i] );
			}

			EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );

			HardwareResult = -1;

			::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_SETCHECK, (LPARAM) BST_CHECKED, 0 );
		}
		break;

	case WM_COMMAND:
		{
			DWORD id = LOWORD( wParam );

			if ( id == IDCANCEL )
			{
				HardwareResult = -1;

				EndDialog( hDlg, 0 );
			}

			if ( id == IDOK )
			{
				EndDialog( hDlg, 0 );
			}

			if ( ( id == IDC_HWID ) && ( HIWORD( wParam ) == CBN_SELENDOK ) )
			{
				BYTE h = ::SendMessage( GetDlgItem( hDlg, IDC_HWID ), CB_GETCURSEL, 0, 0 );

				::SendMessage( GetDlgItem( hDlg, IDC_HWINFO ), LB_RESETCONTENT, 0, 0 );
							
				for (BYTE i=0; i<TZXMaxID[ h ]; i++ )
				{
					const WCHAR *pS = TZXHardwareTypeList[ h ][ i ];

					::SendMessage( GetDlgItem( hDlg, IDC_HWINFO ), LB_ADDSTRING, 0, (LPARAM) pS );
				}

				EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );

				HardwareResult = -1;
			}

			if ( ( id == IDC_HWINFO ) && ( HIWORD( wParam ) == LBN_SELCHANGE ) )
			{
				DWORD HWID = ::SendMessage( GetDlgItem( hDlg, IDC_HWID   ), CB_GETCURSEL, 0, 0 );
				DWORD HWIF = ::SendMessage( GetDlgItem( hDlg, IDC_HWINFO ), LB_GETCURSEL, 0, 0 );
				DWORD HWDT = 0;

				if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 1; }
				if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 2; }
				if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 3; }
				if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 4; }

				HardwareResult = ( HWID << 16 ) | ( HWIF << 8 ) | ( HWDT );

				EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );
			}

			if ( ( id == IDC_HW1 ) || ( id == IDC_HW2 ) || ( id == IDC_HW3 ) || ( id == IDC_HW4 ) )
			{
				if ( HIWORD( wParam ) == BN_CLICKED )
				{
					DWORD HWID = ::SendMessage( GetDlgItem( hDlg, IDC_HWID   ), CB_GETCURSEL, 0, 0 );
					DWORD HWIF = ::SendMessage( GetDlgItem( hDlg, IDC_HWINFO ), LB_GETCURSEL, 0, 0 );
					DWORD HWDT = 0;

					if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW1 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 1; }
					if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW2 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 2; }
					if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW3 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 3; }
					if ( ::SendMessage( GetDlgItem( hDlg, IDC_HW4 ), BM_GETCHECK, 0 ,0 ) == BST_CHECKED ) { HWDT = 4; }

					HardwareResult = ( HWID << 16 ) | ( HWIF << 8 ) | ( HWDT );
				}
			}
		}
		break;
	}

	return FALSE;
}

int PickHardware( void )
{
	HardwareResult = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_HARDWARESELECT ), DialogWnd, HardwareDialogProc, NULL );

	return HardwareResult;
}


INT_PTR CALLBACK ArchiveDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			ArchiveField  = 0;
			ArchiveResult = -1;
			ArchiveString = L"None";

			for (BYTE i=0; i<0x09; i++)
			{
				::SendMessage( GetDlgItem( hDlg, IDC_ARCHIVEFIELD ), CB_ADDSTRING, 0, (LPARAM) TZXArchiveFieldTypes[i] );
			}

			::SendMessage( GetDlgItem( hDlg, IDC_ARCHIVEFIELD ), CB_ADDSTRING, 0, (LPARAM) L"Comment(s)" );

			EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );

			HardwareResult = -1;
		}
		break;

	case WM_COMMAND:
		{
			DWORD id = LOWORD( wParam );

			if ( id == IDCANCEL )
			{
				HardwareResult = -1;

				EndDialog( hDlg, 0 );
			}

			if ( id == IDOK )
			{
				WCHAR vv[256];

				::SendMessage( GetDlgItem( hDlg, IDC_ARCHIVEDATA ), WM_GETTEXT, 255, (LPARAM) vv );

				ArchiveString = vv;
				ArchiveField  = (BYTE) ::SendMessage( GetDlgItem( hDlg, IDC_ARCHIVEFIELD ), CB_GETCURSEL, 0, 0 );

				if ( ArchiveField == 0x09 )
				{
					ArchiveField = 0xFF;
				}

				ArchiveResult = 0;

				EndDialog( hDlg, 0 );
			}

			if ( ( id == IDC_ARCHIVEFIELD ) && ( HIWORD( wParam ) == CBN_SELENDOK ) )
			{
				EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );
			}
		}
		break;
	}

	return FALSE;
}

int PickArchive( void )
{
	ArchiveResult = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_ARCHIVESELECT ), DialogWnd, ArchiveDialogProc, NULL );

	return ArchiveResult;
}

TZXCONFIG_API int StringBox( HWND hWnd, std::wstring Prompt, std::wstring &Result )
{
	DialogWnd = hWnd;

	int r = PickAString( Prompt );

	Result = L"";

	if ( r == 0 )
	{
		Result = StringEntryResponse;
	}

	return r;
}

INT_PTR CALLBACK SelectPathDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			PathSelectResult = -1;

			std::vector<std::wstring>::iterator iPath;

			for ( iPath = pSelectPaths->begin(); iPath != pSelectPaths->end(); iPath++ )
			{
				::SendMessage( GetDlgItem( hDlg, IDC_PATHLIST ), LB_ADDSTRING, 0, (LPARAM) iPath->c_str() );
			}

			EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );

			HardwareResult = -1;
		}
		break;

	case WM_COMMAND:
		{
			DWORD id = LOWORD( wParam );

			if ( id == IDCANCEL )
			{
				PathSelectResult = -1;

				EndDialog( hDlg, 0 );
			}

			if ( id == IDOK )
			{
				PathSelectResult  = ::SendMessage( GetDlgItem( hDlg, IDC_PATHLIST ), LB_GETCURSEL, 0, 0 );

				EndDialog( hDlg, 0 );
			}

			if ( ( id == IDC_PATHLIST ) && ( HIWORD( wParam ) == LBN_SELCHANGE ) )
			{
				EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );
			}
		}
		break;
	}

	return FALSE;
}

TZXCONFIG_API int TZXSelectPath( HWND hWnd, std::vector<std::wstring> &Paths )
{
	pSelectPaths = &Paths;

	DialogWnd = hWnd;

	PathSelectResult = -1;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_PATHSELECT ), DialogWnd, SelectPathDialogProc, NULL );

	return PathSelectResult;
}