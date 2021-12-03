#include "stdafx.h"

#include "IconResolve.h"
#include "SpriteFile.h"
#include "Sprite.h"

#include <map>

std::map<WORD, IconDef> ResolvedFileTypes;

WORD GetFiletypeSuffix( BYTE *pName )
{
	if ( rstrnlen( pName, 8 ) < 8 )
	{
		return 0;
	}

	WORD r = 0;

	for ( int i=5; i<8; i++ )
	{
		r <<= 4;

		if ( ( pName[ i ] >= '0' ) && ( pName[ i ] <= '9' ) ) { r |=   pName[ i ] - '0'; }
		if ( ( pName[ i ] >= 'A' ) && ( pName[ i ] <= 'F' ) ) { r |= ( pName[ i ] - 'A' ) + 10; }
		if ( ( pName[ i ] >= 'a' ) && ( pName[ i ] <= 'f' ) ) { r |= ( pName[ i ] - 'a' ) + 10; }
	}

	return r;
}

void ResolveAppIcon( FileSystem *pFS, NativeFile *iFile )
{
	if ( !pFS->UseResolvedIcons )
	{
		return;
	}

	DWORD MaskColour = GetSysColor( COLOR_WINDOW );

	NativeFileIterator iFile2;

	if ( ( iFile->Flags & FF_Directory ) && ( iFile->Filename[ 0 ] == '!' ) )
	{
		FileSystem *pTempFS = pFS->Clone( );

		pTempFS->ChangeDirectory( iFile->fileID );

		NativeFileIterator iSpriteFile;

		NativeFile *pSpriteFile;

		DWORD PrefSpriteFileId = 0xFFFFFFFF;

		for ( iSpriteFile = pTempFS->pDirectory->Files.begin(); iSpriteFile != pTempFS->pDirectory->Files.end(); iSpriteFile++)
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
			pSpriteFile = &( pTempFS->pDirectory->Files[ PrefSpriteFileId ] );

			DataSource *pSpriteSource = pTempFS->FileDataSource( pSpriteFile->fileID );

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

				if ( rstrnicmp( iSprite->Filename, (BYTE *) "file_", 5 ) )
				{
					WORD filetype = GetFiletypeSuffix( iSprite->Filename );

					if ( filetype != 0 )
					{
						// Delete any existing
						std::map<WORD, IconDef>::iterator iIcon = ResolvedFileTypes.find( filetype );

						if ( iIcon != ResolvedFileTypes.end() )
						{
							free( ResolvedFileTypes[ filetype ].pImage );

							ResolvedFileTypes.erase( iIcon );
						}

						CTempFile FileObj;

						spriteFile.ReadFile( iSprite->fileID, FileObj );

						Sprite sprite( FileObj );

						IconDef icon;

						sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

						icon.Aspect = sprite.SpriteAspect;

						if ( sprite.Valid() )
						{
							ResolvedFileTypes[ filetype ] = icon;
						}

						/* Rescan the directory for this type */
						for (iFile2=pFS->pDirectory->Files.begin(); iFile2!=pFS->pDirectory->Files.end(); iFile2++)
						{
							if ( ( iFile2->RISCTYPE == filetype ) && ( !iFile2->HasResolvedIcon ) )
							{
								IconDef DupIcon;

								DupIcon.pImage = malloc( icon.bmi.biSizeImage );
										
								memcpy( DupIcon.pImage, icon.pImage, icon.bmi.biSizeImage );

								DupIcon.bmi    = icon.bmi;
								DupIcon.Aspect = icon.Aspect;

								pFS->pDirectory->ResolvedIcons[ iFile2->fileID ] = DupIcon;

								iFile2->HasResolvedIcon = true;
							}
						}
					}
				}
			}

			DS_RELEASE( pSpriteSource );
		}

		delete pTempFS;
	}

	// Look for existing type
	if ( !( iFile->Flags & FF_Directory ) )
	{
		std::map<WORD, IconDef>::iterator iIcon = ResolvedFileTypes.find( iFile->RISCTYPE );

		if ( ( iIcon != ResolvedFileTypes.end() ) && ( !iFile->HasResolvedIcon ) )
		{
			IconDef DupIcon;

			DupIcon.pImage = malloc( iIcon->second.bmi.biSizeImage );
										
			memcpy( DupIcon.pImage, iIcon->second.pImage, iIcon->second.bmi.biSizeImage );

			DupIcon.bmi    = iIcon->second.bmi;
			DupIcon.Aspect = iIcon->second.Aspect;

			pFS->pDirectory->ResolvedIcons[ iFile->fileID ] = DupIcon;

			iFile->HasResolvedIcon = true;
		}
	}
}

int ResolveAppIcons( FileSystem *pFS )
{
	if ( !pFS->UseResolvedIcons )
	{
		return 0;
	}

	NativeFileIterator iFile;

	for (iFile=pFS->pDirectory->Files.begin(); iFile!=pFS->pDirectory->Files.end(); iFile++)
	{
		ResolveAppIcon( pFS, &*iFile );
	}

	return 0;
}
