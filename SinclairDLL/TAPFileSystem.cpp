#include "StdAfx.h"
#include "SinclairDefs.h"
#include "TAPFileSystem.h"


TAPFileSystem::TAPFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) 
{
	pSource = pDataSource;

	FSID = FSID_SPECTRUM_TAP;

	Flags = FSF_Size | FSF_Supports_Spaces | FSF_Reorderable;
}


TAPFileSystem::~TAPFileSystem(void)
{

}

FSHint TAPFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension == nullptr )
	{
		return hint;
	}

	if ( rstrncmp( Extension, (BYTE *) "TAP", 3 ) )
	{
		hint.FSID       = FSID_SPECTRUM_TAP;
		hint.Confidence = 20;

		WORD FirstHeaderLen;

		pSource->ReadRaw( 0, 1, (BYTE *) &FirstHeaderLen );

		if ( FirstHeaderLen == 0x13 )
		{
			/* This indicates a spectrum header has been found - but this is not conclusive!
			
			   This could be a TAP file that uses custom loaders, and we're looking side B
			   which is data, and not intended to be loaded directly. */

			hint.Confidence = 30;
		}
	}

	return hint;
}

BYTE *TAPFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE status[ 128 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );

		return status;
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );

		return status;
	}
	
	switch ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] )
	{
	case 0:
		{
			if ( pDirectory->Files[ FileIndex ].Attributes[ 3 ] < 32768 )
			{
				rsprintf(
					status, "Program: %s - %04X bytes LINE %d, vars: %04X",
					pDirectory->Files[ FileIndex ].Filename,
					(DWORD) pDirectory->Files[ FileIndex ].Length,
					pDirectory->Files[ FileIndex ].Attributes[ 3 ],
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
			else
			{
				rsprintf(
					status, "Program: %s - %04X bytes, vars: %04X",
					pDirectory->Files[ FileIndex ].Filename,
					(DWORD) pDirectory->Files[ FileIndex ].Length,
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
		}
		break;

	case 1:
		{
			rsprintf(
				status, "Number Array: %s - %04X bytes of %c$()",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 2:
		{
			rsprintf(
				status, "Character Array: %s - %04X bytes of %c$()",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 3:
		{
			rsprintf(
				status, "Bytes: %s - %04X bytes at %04X",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				pDirectory->Files[ FileIndex ].Attributes[ 1 ]
			);
		}
		break;

	default:
		{
			rsprintf(
				status, "%s - %04X bytes",
				pDirectory->Files[ FileIndex ].Filename,
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;
	}

	return status;
}

BYTE *TAPFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[1024];

	switch ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] )
	{
	case 0:
		{
			if ( pDirectory->Files[ FileIndex ].Attributes[ 3 ] < 32768 )
			{
				rsprintf(
					status, "Program: LINE %d, vars at offset %04X",
					pDirectory->Files[ FileIndex ].Attributes[ 3 ],
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
			else
			{
				rsprintf(
					status, "Program: Vars at offset %04X",
					pDirectory->Files[ FileIndex ].Attributes[ 4 ]
				);
			}
		}
		break;

	case 1:
		{
			rsprintf(
				status, "Number Array: %04X bytes of %c$()",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 2:
		{
			rsprintf(
				status, "Character Array: %04X bytes of %c$()",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				( pDirectory->Files[ FileIndex ].Attributes[ 5 ] & 31 ) + 'a'
			);
		}
		break;

	case 3:
		{
			rsprintf(
				status, "Bytes: %04X bytes at %04X",
				(DWORD) pDirectory->Files[ FileIndex ].Length,
				pDirectory->Files[ FileIndex ].Attributes[ 1 ]
			);
		}
		break;

	default:
		{
			rsprintf(
				status, "Orphaned block (No header) - %04X bytes",
				UString( (char *) pDirectory->Files[ FileIndex ].Filename ),
				(DWORD) pDirectory->Files[ FileIndex ].Length
			);
		}
		break;
	}

	return status;
}


BYTE *TAPFileSystem::GetTitleString( NativeFile *pFile = nullptr )
{
	static BYTE title[ 1024 ];
	
	if ( pFile != nullptr )
	{
		rsprintf( title, "%s", pFile->Filename );
	}
	else
	{
		rsprintf( title, (BYTE *) "TAP Image" );
	}

	return title;
}


int TAPFileSystem::ReadFile(DWORD FileID, CTempFile &store) {
	BYTE *pBuffer = (BYTE *) malloc( (size_t) pDirectory->Files[ FileID ].Length );

	pSource->ReadRaw(
		pDirectory->Files[ FileID ].Attributes[ 0 ] + 3,
		(DWORD) pDirectory->Files[ FileID ].Length,
		pBuffer
	);

	store.Seek( 0 );
	store.Write( pBuffer, (DWORD) pDirectory->Files[ FileID ].Length );

	return 0;
}

int	TAPFileSystem::WriteFile(NativeFile *pFile, CTempFile &store) {
	return -1;
}


AttrDescriptors TAPFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Offset in TAP file. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex;
	Attr.Name  = L"TAP Offset";
	Attrs.push_back( Attr );

	/* File Type */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrWarning | AttrFile;
	Attr.Name  = L"File Type";
	
	AttrOption opt;

	opt.Dangerous       = false;
	opt.EquivalentValue = 0;
	opt.Name            = L"Program";
	
	Attr.Options.push_back( opt );

	opt.EquivalentValue = 1;
	opt.Name            = L"Number Array";

	Attr.Options.push_back( opt );

	opt.EquivalentValue = 2;
	opt.Name            = L"Character Array";

	Attr.Options.push_back( opt );

	opt.EquivalentValue = 3;
	opt.Name            = L"Bytes";

	Attr.Options.push_back( opt );

	opt.Dangerous       = true;
	opt.EquivalentValue = 0xFFFFFF;
	opt.Name            = L"Orphaned Block";

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );

	/* Load addr for bytes */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load Address";
	Attrs.push_back( Attr );

	/* Auto start line */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrDec | AttrWarning | AttrFile;
	Attr.Name  = L"Auto start line";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Vars Offset";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrDanger | AttrFile;;
	Attr.Name  = L"Variable ID";
	Attrs.push_back( Attr );

	return Attrs;
}
