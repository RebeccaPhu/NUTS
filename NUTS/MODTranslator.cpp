#include "StdAfx.h"
#include "MODTranslator.h"
#include "MikModNUTSRawDrv.h"
#include "libfuncs.h"
#include "NUTSError.h"

bool MODTranslator::Inited = false;
bool MODTranslator::HaveLock = false;

MODTranslator::MODTranslator(void)
{
	if ( !HaveLock )
	{
		InitializeCriticalSection( &MikModLock );
	}

	HaveLock = true;
}

MODTranslator::~MODTranslator(void)
{
}

int MODTranslator::Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx )
{
	EnterCriticalSection( &MikModLock );

	if ( !Inited )
	{
		MikMod_RegisterDriver( &drv_nuts_raw );

		MikMod_RegisterAllLoaders();
	}

	pMODInternals->trgObj = &Output;

	tx->FormatName = L"MOD";

	md_mode |= DMODE_SOFT_MUSIC;

	int MM = MikMod_Init("");

	WORD  SongPos = 0;
	QWORD SPExt   = 0;

	AudioCuePoint cue;

	cue.Name        = L"Song Position " + std::to_wstring( (long double) SongPos );
	cue.Position    = 0;
	cue.UseEncoding = false;

	if ( MM == 0 )
	{
		Inited = true;

		MODULE *module = Player_Load( AString( (WCHAR *) obj.Name().c_str() ), 64, 0 );

		if ( module != nullptr )
		{
			tx->Title.Name        = UString( (char *) module->songname );
			tx->Title.UseEncoding = false;
			tx->CuePoints.clear();
			tx->CuePoints.push_back( cue );
			tx->WideBits          = true;
			tx->Stereo            = true;

			module->loop = FALSE;
			module->wrap = FALSE;

			Player_Start( module );

			bool NoLoopKill = false;

			while ( ( Player_Active() ) && ( WaitForSingleObject( tx->hStopTranslating, 0 ) == WAIT_TIMEOUT ) )
			{
				if ( module->sngpos > SongPos )
				{
					SongPos = module->sngpos;
					
					cue.Name     = L"Song Position " + std::to_wstring( (long double) SongPos );
					cue.Position = Output.Ext();

					tx->CuePoints.push_back( cue );

					SPExt = Output.Ext();
				}

				MikMod_Update();

				if ( ( Output.Ext() - SPExt ) > 10584000 )
				{
					// over 1 minute since the last song pos change. It's looping.
					SetEvent( tx->hStopTranslating );
				}

				if ( ( Output.Ext() > 211680000 ) && ( !NoLoopKill) ) // 20 minutes of audio
				{
					if ( MessageBox( tx->hParentWnd,
						L"The NUTS MOD translator has generated over 200 MB of data, equating to over 20 minutes of audio. "
						L"This could indicate the file is looping in a way that the MiKMod decoder library is unable to detect. "
						L"If this MOD file is unusually large, this could be normal. If you allow the processing to continue "
						L"it could use all of your available disk space and make your system unstable. You will not be asked "
						L"this question again.\r\n\r\n"
						L"Do you wish to continue processing the MOD audio data?",
						L"MOD File Loop detected",
						MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2
						) == IDNO )
					{
						SetEvent( tx->hStopTranslating );
					}
					else
					{
						NoLoopKill = true;
					}
				}
			}

			Player_Stop();
			Player_Free(module);
		}

		MikMod_Exit();

		tx->AudioSize = Output.Ext();
	}
	else
	{
		LeaveCriticalSection( &MikModLock );

		return NUTSError( 0x600, UString( (char *) MikMod_strerror( MikMod_errno ) ) );
	}

	LeaveCriticalSection( &MikModLock );

	return 0;
}