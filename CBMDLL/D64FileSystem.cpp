#include "StdAfx.h"
#include "D64FileSystem.h"

int	D64FileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	//	See notes in D64Directory regarding file trapesing.

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	foffset	= 0;
	int	logsect	= pFile->Attributes[ 0 ];

	unsigned char sector[256];

	D64Directory *pd = (D64Directory *) pDirectory;

	while (logsect != -1) {
		pSource->ReadSector(logsect, sector, 256);

		if (sector[0] != 0) {
			store.Write( &sector[2], 254 );

			logsect	= pd->SectorForLink(sector[0], sector[1]);

			foffset	+= 254;
		} else {
			store.Write( &sector[2], (DWORD) sector[1] );

			logsect	= -1;
		}
	}

	return 0;
}

int	D64FileSystem::WriteFile(NativeFile *pFile, CTempFile &store, char *Filename)
{
	return 0;
}

BYTE *D64FileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[40];

	if ((FileIndex < 0) || ((unsigned) FileIndex > pDirectory->Files.size()))
	{
		status[0] = 0;

		return status;
	}

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	rsprintf( status, "%d BYTES, %s, %s",
		pDirectory->Files[FileIndex].Length,
//		(pDirectory->Files[FileIndex].Locked)?"Locked":"Not Locked",
//		(pDirectory->Files[FileIndex].Read)?"Closed":"Not Closed (Splat)"
		"",""
		);

	return status;
}

BYTE *D64FileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[ 128 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d FILES", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d FILES SELECTED", SelectedItems );
	}
	else 
	{
		rsprintf( status, "%s.%s - %d BYTES, %s, %s",
		pDirectory->Files[FileIndex].Filename,
		pDirectory->Files[FileIndex].Extension,
		(DWORD) pDirectory->Files[FileIndex].Length,
//		(pDirectory->Files[FileIndex].Locked)?"Locked":"Not Locked",
//		(pDirectory->Files[FileIndex].Read)?"Closed":"Not Closed (Splat)"
		"",""
		);
	}

	return status;
}

FSHint D64FileSystem::Offer( BYTE *Extension )
{
	//	D64s have a somewhat "optimized" layout that doesn't give away any reliable markers that the file
	//	is indeed a D64. So we'll just have to check the extension and see what happens.

	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension != nullptr )
	{
		if ( _strnicmp( (char *) Extension, "D64", 3 ) == 0 )
		{
			hint.Confidence = 20;
			hint.FSID       = FSID_D64;
		}
	}

	return hint;
}


BYTE *D64FileSystem::GetTitleString( NativeFile *pFile )
{
	static BYTE title[512];
	
	if ( pFile == nullptr )
	{
		unsigned char	d64cache[256];
//		unsigned char	disktitle[17];

		//	Read the BAM at sector 357
		pSource->ReadSector(357, d64cache, 256);

		D64Directory *pd = (D64Directory *) pDirectory;

		strncpy_s( (char *) title, 512, (char *) "D64::", 5 );
		strncpy_s( (char *) &title[ 5 ], 512, (char *) &d64cache[0x90], 16 );
	}
	else
	{
		sprintf_s( (char *) title, 512, "%s.%s", (char *) pFile->Filename, (char *) pFile->Extension );
	}

	return title;
}


AttrDescriptors D64FileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex;
	Attr.Name  = L"Start sector";
	Attrs.push_back( Attr );

	/* Locked */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Locked";
	Attrs.push_back( Attr );

	/* Closed */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Closed";
	Attrs.push_back( Attr );

	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrWarning | AttrFile;
	Attr.Name  = L"File type";

	AttrOption opt;

	opt.Name            = L"Deleted (DEL)";
	opt.EquivalentValue = 0x00000000;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Sequential (SEQ)";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Program (PRG)";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"User (USR)";
	opt.EquivalentValue = 0x00000003;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Relative (REL)";
	opt.EquivalentValue = 0x00000004;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );
		

	return Attrs;
}