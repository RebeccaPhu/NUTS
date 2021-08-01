#pragma once

#include "ADFSDirectoryCommon.h"
#include "../nuts/directory.h"
#include "../NUTS/libfuncs.h"
#include "NewFSMap.h"
#include "Defs.h"

class ADFSEDirectory : public Directory, public ADFSDirectoryCommon
{

public:
	ADFSEDirectory::ADFSEDirectory(DataSource *pDataSource) : Directory(pDataSource) {
		DirSector    = 0;
		ParentSector = 0;

		BigDirName   = nullptr;
	}

	ADFSEDirectory( const ADFSEDirectory &source ) : Directory( source.pSource )
	{
		Files         = source.Files;
		
		ResolvedIconList::const_iterator iIcon;

		for ( iIcon = source.ResolvedIcons.begin(); iIcon != source.ResolvedIcons.end(); iIcon++ )
		{
			DWORD   nIcon = iIcon->first;
			IconDef Icon  = iIcon->second;

			void *pDupImage = malloc( Icon.bmi.biSizeImage );

			memcpy( pDupImage, Icon.pImage, Icon.bmi.biSizeImage );

			Icon.pImage = pDupImage;

			ResolvedIcons[ nIcon ] = Icon;
		}

		pMap = source.pMap;

		MediaShape = source.MediaShape;

		DirSector    = source.DirSector;
		ParentSector = source.ParentSector;

		if ( source.BigDirName != nullptr )
		{
			BigDirName = rstrndup( source.BigDirName, 256 );
		}
		else
		{
			BigDirName = nullptr;
		}

		MasterSeq = source.MasterSeq;
		
		memcpy( DirTitle, source.DirTitle, 19 );
		memcpy( DirName, source.DirName, 19 );

		SecSize = source.SecSize;
	}

	~ADFSEDirectory(void)
	{
		if ( BigDirName != nullptr )
		{
			free( BigDirName );
		}
	}

	int	ReadDirectory(void);
	int	ReadEDirectory(void);
	int	ReadPDirectory(void);

	int	WriteDirectory(void);
	int	WriteEDirectory(void);
	int	WritePDirectory(void);

	NewFSMap *pMap;

	DWORD DirSector;
	DWORD ParentSector;

	BYTE MasterSeq;
	BYTE DirTitle[ 20 ];
	BYTE DirName[  20 ];

	BYTE *BigDirName;

	DWORD SecSize;

	void ReplaceSource( DataSource *pSrc )
	{
		pSource = pSrc;
	}

private:
	void TranslateType( NativeFile *file );
};

