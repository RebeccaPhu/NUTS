#pragma once

#include "../NUTS/Defs.h"
#include "../NUTS/TempFile.h"
#include "../NUTS/Directory.h"

class ADFSCommon
{
public:
	ADFSCommon(void);
	~ADFSCommon(void);

protected:
	int  ExportSidecar( NativeFile *pFile, SidecarExport &sidecar );
	int  ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj );

	template <class T> int ResolveAppIcons( T *pFS )
	{
		if ( !CommonUseResolvedIcons )
		{
			return 0;
		}

		DWORD MaskColour = GetSysColor( COLOR_WINDOW );

		NativeFileIterator iFile;

		for (iFile=pFS->pDirectory->Files.begin(); iFile!=pFS->pDirectory->Files.end(); iFile++)
		{
			if ( iFile->Flags & FF_Directory )
			{
				T TempFS( *pFS );

				TempFS.ChangeDirectory( iFile->fileID );

				NativeFileIterator iSpriteFile;

				NativeFile *pSpriteFile;

				DWORD PrefSpriteFileId = 0xFFFFFFFF;

				for ( iSpriteFile = TempFS.pDirectory->Files.begin(); iSpriteFile != TempFS.pDirectory->Files.end(); iSpriteFile++)
				{
					if ( rstricmp( iSpriteFile->Filename, (BYTE *) "!Sprites" ) )
					{
						if ( PrefSpriteFileId == 0xFFFFFFFF )
						{
							PrefSpriteFileId = iSpriteFile->fileID;
						}
					}

					if ( rstricmp( iSpriteFile->Filename, (BYTE *) "!Sprites22" ) )
					{
						PrefSpriteFileId = iSpriteFile->fileID;
					}
				}

				if ( PrefSpriteFileId != 0xFFFFFFFF )
				{
					pSpriteFile = &( TempFS.pDirectory->Files[ PrefSpriteFileId ] );

					DataSource *pSpriteSource = TempFS.FileDataSource( pSpriteFile->fileID );

					SpriteFile spriteFile( pSpriteSource );

					spriteFile.Init();

					NativeFileIterator iSprite;

					for ( iSprite = spriteFile.pDirectory->Files.begin(); iSprite != spriteFile.pDirectory->Files.end(); iSprite++ )
					{
						if ( rstricmp( iSprite->Filename, iFile->Filename ) )
						{
							CTempFile FileObj;

							spriteFile.ReadFile( iSprite->fileID, FileObj );

							Sprite sprite( FileObj );

							IconDef icon;

							sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

							icon.Aspect = sprite.SpriteAspect;

							if ( sprite.Valid() )
							{
								pFS->pDirectory->ResolvedIcons[ iFile->fileID ] = icon;

								iFile->HasResolvedIcon = true;
							}
						}
					}

					pSpriteSource->Release();
				}
			}
		}

		return 0;
	}

	void FreeAppIcons( Directory *pDirectory );

	NativeFile OverrideFile;

	bool Override;

	bool CommonUseResolvedIcons;

	void InterpretImportedType( NativeFile *pFile );
	void InterpretNativeType( NativeFile *pFile );
	void SetTimeStamp( NativeFile *pFile );
};

