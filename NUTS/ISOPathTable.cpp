#include "StdAfx.h"
#include "ISOPathTable.h"
#include "libfuncs.h"
#include "ISOFuncs.h"

#include <algorithm>

ISOPathTable::ISOPathTable( DataSource *pSrc, DWORD dwSectorSize )
{
	DS_RETAIN( pSrc );

	pSource    = pSrc;

	SectorSize = dwSectorSize;
}


ISOPathTable::~ISOPathTable(void)
{
	DS_RELEASE( pSource );
}


void ISOPathTable::ReadPathTable( DWORD Extent, DWORD Size, FSIdentifier FSID, bool IsJoliet )
{
	// This functon rather naïvely assumes the path table is an L-Table,
	// as it will be called by ISO19960FileSystem to read the primary L-Table,
	// even if it's written back as an M-Table.

	DWORD TableSects = ( Size + ( SectorSize -1 ) ) / SectorSize;

	if ( TableSects >= 0x1000 )
	{
		// No path table is this huge. Rational for the number:
		// Max sectors on a CDROM is 345,000 = ~0x55000 = max number of directories, assuming all dirs an no files.
		// Let's say, on average 80 directories per path table sector in worst case (big filenames)
		// That gives us about 0x1000 sectors to hold the path table.
		return;
	}

	AutoBuffer Table( TableSects * SectorSize );

	BYTE *pTable = (BYTE *) Table;

	for ( DWORD i=0; i<TableSects; i++ )
	{
		if ( pSource->ReadSectorLBA( Extent + i, &pTable[ i * SectorSize ], SectorSize ) != DS_SUCCESS )
		{
			return;
		}
	}

	DWORD TablePtr = 0;

	TableEntries.clear();

	WORD DirNum = 1;
	BYTE FNSize = 0;

	while ( TablePtr < Size )
	{
		ISOPathTableEntry entry;

		BYTE *pEnt = &pTable[ TablePtr ];

		if ( FSID == FSID_ISO9660 )
		{
			FNSize = pEnt[ 0];

			if ( ( FNSize == 1 ) && ( pEnt[ 8 ] == 0 ) )
			{
				// Root dir
				entry.IsRoot = true;
				entry.ParentExtent = LEDWORD( &pEnt[ 2 ] ); // Same as its own extent
			}
			else
			{
				entry.IsRoot        = false;

				if ( IsJoliet )
				{
					BYTE BEStrs[ 256 ];

					static  BOOL UseDefault = TRUE;

					WORD Src[ 256 ];
					WORD *pSrc = (WORD *) &pEnt[ 8 ];

					// WHY BIGENDIAN UNICODE FOR THE LOVE OF
					for ( WORD i=0; i<FNSize>>1; i++ )
					{
						Src[ i ] = BEWORD( (BYTE *) &pSrc[ i ] );
					}

					WideCharToMultiByte( GetACP(), NULL, (WCHAR *) Src, FNSize >> 1, (char *) BEStrs, 256, "_", &UseDefault );

					entry.DirectoryName = BYTEString( BEStrs );
				}
				else
				{
					entry.DirectoryName = BYTEString( &pEnt[ 8 ], FNSize );
				}
			}

			entry.Extent       = LEDWORD( &pEnt[ 2 ] );
			entry.ParentNum    = LEWORD(  &pEnt[ 6 ] );
		}
		else
		{
			FNSize = pEnt[ 5 ];

			if ( ( FNSize == 1 ) && ( pEnt[ 8 ] == 0 ) )
			{
				// Root dir
				entry.IsRoot = true;
				entry.ParentExtent = LEDWORD( &pEnt[ 0 ] ); // Same as its own extent
			}
			else
			{
				entry.IsRoot        = false;
				entry.DirectoryName = BYTEString( &pEnt[ 8 ], FNSize );
			}

			entry.Extent       = LEDWORD( &pEnt[ 0 ] );
			entry.ParentNum    = LEWORD(  &pEnt[ 6 ] );
		}

		entry.DirectoryNum = DirNum;

		if ( ( entry.ParentNum > 0 ) && ( entry.ParentNum <= TableEntries.size() ) )
		{
			entry.ParentExtent = TableEntries[ entry.ParentNum - 1 ].Extent;
		}
		else
		{
			// ?
			if ( !entry.IsRoot )
			{
				entry.ParentExtent = 0;
			}
		}

		TablePtr += 8 + FNSize;

		if ( FNSize & 1 )
		{
			TablePtr++;
		}

		TableEntries.push_back( entry );

		DirNum++;
	}

	CompilePathTree();
}

void ISOPathTable::CompilePathTree()
{
	bool MoreEntries = true;

	PathTree.clear();

	PathTree.push_back( ISOPathTableEntries() );

	for ( ISOPathTableIterator iTable = TableEntries.begin(); iTable != TableEntries.end(); iTable++ )
	{
		if ( iTable->IsRoot ) // Don't store the root
		{
			RootExtent = iTable->Extent;

			continue;
		}

		if ( iTable->ParentNum == 1 ) // Root is parent
		{
			PathTree[ 0 ].push_back( *iTable );
		}
	}

	DWORD Level = 0;

	while ( MoreEntries )
	{
		MoreEntries = false;

		// Go through the current level, and get directory numbers.
		for ( ISOPathTableIterator iLevelTable = PathTree[ Level ].begin(); iLevelTable != PathTree[ Level ].end(); iLevelTable++ )
		{
			if ( PathTree.size() <= ( Level  + 1 ) )
			{
				PathTree.push_back( ISOPathTableEntries() );
			}

			// Go through the path table again looking for dirs that are subdirs of these
			for ( ISOPathTableIterator iTable = TableEntries.begin(); iTable != TableEntries.end(); iTable++ )
			{
				if ( iTable->ParentNum == iLevelTable->DirectoryNum )
				{
					PathTree[ Level + 1 ].push_back( *iTable );

					MoreEntries = true;
				}
			}
		}

		Level++;
	}

	// Inevitably, the bottom "level" table will be empty.
	if ( PathTree.back().size() == 0 )
	{
		PathTree.pop_back();
	}
}

void ISOPathTable::WritePathTable( DWORD Extent, bool IsMTable, FSIdentifier FSID, bool IsJoliet )
{
	DWORD Size       = GetProjectedSize();
	DWORD TableSects = ( Size + ( SectorSize -1 ) ) / SectorSize;

	AutoBuffer Table( TableSects * SectorSize );

	BYTE *pTable = (BYTE *) Table;

	ZeroMemory( pTable, TableSects * SectorSize );

	DWORD TablePtr = 0;

	for ( ISOPathTableIterator iPath = TableEntries.begin(); iPath != TableEntries.end(); iPath++ )
	{
		BYTE FNSize = 0;

		BYTE *pEnt = &pTable[ TablePtr ];

		if ( FSID == FSID_ISO9660 )
		{
			if ( iPath->IsRoot )
			{
				pEnt[ 0 ] = 1;
				pEnt[ 8 ] = 0;
			}
			else
			{
				if ( IsJoliet )
				{
					WCHAR *pChars = UString( (char *) iPath->DirectoryName );
					WORD  BEStr[ 256 ];

					for ( WORD i=0; i<iPath->DirectoryName.length(); i++ )
					{
						WBEWORD( (BYTE *) &BEStr[ i ], pChars[ i ] );
					}

					pEnt[ 0 ] = iPath->DirectoryName.length() << 1;
					memcpy( &pEnt[ 8 ], BEStr, pEnt[ 0 ] );
				}
				else
				{
					pEnt[ 0 ] = iPath->DirectoryName.length();
					memcpy( &pEnt[ 8 ], (BYTE *) iPath->DirectoryName, iPath->DirectoryName.length() );
				}
			}

			FNSize = pEnt[ 0 ];

			pEnt[ 1 ] = 0;

			if ( IsMTable )
			{
				WBEDWORD( &pEnt[ 2 ], iPath->Extent );
				WBEWORD(  &pEnt[ 6 ], iPath->ParentNum );
			}
			else
			{
				WLEDWORD( &pEnt[ 2 ], iPath->Extent );
				WLEWORD(  &pEnt[ 6 ], iPath->ParentNum );
			}
		}
		else
		{
			if ( iPath->IsRoot )
			{
				pEnt[ 5 ] = 1;
				pEnt[ 8 ] = 0;
			}
			else
			{
				pEnt[ 5 ] = iPath->DirectoryName.length();
				memcpy( &pEnt[ 8 ], (BYTE *) iPath->DirectoryName, iPath->DirectoryName.length() );
			}

			FNSize = pEnt[ 5 ];

			pEnt[ 4 ] = 0;

			if ( IsMTable )
			{
				WBEDWORD( &pEnt[ 0 ], iPath->Extent );
				WBEWORD(  &pEnt[ 6 ], iPath->ParentNum );
			}
			else
			{
				WLEDWORD( &pEnt[ 0 ], iPath->Extent );
				WLEWORD(  &pEnt[ 6 ], iPath->ParentNum );
			}
		}

		TablePtr += 8 + FNSize;

		if ( FNSize & 1 )
		{
			TablePtr++;
		}
	}

	for ( DWORD i=0; i<TableSects; i++ )
	{
		SetISOHints( pSource, ( i == ( TableSects - 1 ) ), ( i == ( TableSects - 1 ) ) );

		if ( pSource->WriteSectorLBA( Extent + i, &pTable[ i * SectorSize ], SectorSize ) != DS_SUCCESS )
		{
			return;
		}
	}
}

DWORD ISOPathTable::GetProjectedSize( bool Joliet )
{
	DWORD TableSize = 0;

	// NOTE! Unlike directory records, path tables can have entries crossing sector boundaries. This makes
	// this calculation easier, but makes the opeations fantastically harder.

	for ( ISOPathTableIterator iPath = TableEntries.begin(); iPath != TableEntries.end(); iPath++ )
	{
		TableSize += 8; // All entries are at least this.

		size_t FBytes = iPath->DirectoryName.length();

		if ( Joliet ) { FBytes <<= 1; }

		TableSize += (DWORD) FBytes; // Entries must be an even size.

		if ( FBytes & 1 )
		{
			TableSize++;
		}
	}

	return TableSize;
}

void ISOPathTable::CompilePathTable()
{
	// Convert the tree structures back to a flat list. We'll need a map to hold some directory number mappings.
	std::map<DWORD,WORD> DirectoryNumberMap;

	// Preserve the root, which is the first entry.
	ISOPathTableEntry root = TableEntries.front();

	TableEntries.clear();

	TableEntries.push_back( root );

	for ( int i = 0; i<PathTree.size(); i++ )
	{
		for ( ISOPathTableIterator iPath = PathTree[ i ].begin(); iPath != PathTree[ i ].end(); iPath++ )
		{
			TableEntries.push_back( *iPath );
		}
	}

	// Now we have a flat list in the right order, extract some directory numbers
	WORD dn = 1;

	for (  ISOPathTableIterator iPath = TableEntries.begin(); iPath != TableEntries.end(); iPath++ )
	{
		iPath->DirectoryNum = dn;
		
		DirectoryNumberMap[ iPath->Extent ] = dn;

		dn++;
	}

	// Now go back and assign the parent nums
	for (  ISOPathTableIterator iPath = TableEntries.begin(); iPath != TableEntries.end(); iPath++ )
	{
		iPath->ParentNum = DirectoryNumberMap[ iPath->ParentExtent ];
	}

	// All done.
}

bool PathTableSort( ISOPathTableEntry &a, ISOPathTableEntry &b )
{
	BYTE DirNameA[ 256 ];
	BYTE DirNameB[ 256 ];

	// Yeah ok, this is lazy. but is saves rolling another function to do this.
	NativeFile aa;
	NativeFile bb;

	aa.Filename = a.DirectoryName; aa.Flags = FF_Directory;
	bb.Filename = b.DirectoryName; bb.Flags = FF_Directory;

	MakeISOFilename( &aa, DirNameA );
	MakeISOFilename( &bb, DirNameB );

	// std::sort wants true if a < b
	if ( a.ParentNum < b.ParentNum )
	{
		return true;
	}
	else if ( a.ParentNum > b.ParentNum )
	{
		return false;
	}

	// Same parent, check filenames.
	for ( int i=0; i<min(a.DirectoryName.length(), b.DirectoryName.length() ); i++ )
	{
		if ( DirNameA[ i ] < DirNameB[ i ] )
		{
			return true;
		}
		else if ( DirNameA[ i ] > DirNameB[ i ] )
		{
			return false;
		}
	}

	// If we got here then the directories have the same name up to the point of the minimum length of the two.
	if ( a.DirectoryName.length() < b.DirectoryName.length() )
	{
		return true;
	}

	return false;
}

void ISOPathTable::AddDirectory( BYTEString &Identifier, DWORD Extent, DWORD ParentExt )
{
	// The technique here is to go through each level of the path tree until we find
	// the level containing the parent directory number. Then we add 1, and shunt the
	// new entry onto the next level's path subtable.

	bool Done          = false;
	int  InsertedLevel = 0;

	if ( ( ParentExt == RootExtent ) || ( PathTree.size() == 0 ) )
	{
		// Subdirectory of the root. Since there isn't a -1 level, whack it on the end.
		ISOPathTableEntry entry;
				
		entry.DirectoryName = Identifier;
		entry.DirectoryNum  = 0;
		entry.ParentNum     = 1;
		entry.ParentExtent  = RootExtent;
		entry.Extent        = Extent;
		entry.IsRoot        = false;

		// There may not be a level 0 yet, so create one if needed.
		if ( PathTree.size() == 0 )
		{
			PathTree.push_back( ISOPathTableEntries() );
		}

		PathTree[ 0 ].push_back( entry );
	}
	else
	{
		for ( int i=0; i<PathTree.size(); i++)
		{
			for ( ISOPathTableIterator iTable = PathTree[ i ].begin(); iTable != PathTree[ i ].end(); iTable++ )
			{
				if ( iTable->Extent == ParentExt )
				{
					// We've found the level that the new directory's parent exists at.
					// Construct a table entry and add it on the end of the next level.
					// The directory number of the new entry doesn't matter yet.
					ISOPathTableEntry entry;
				
					entry.DirectoryName = Identifier;
					entry.DirectoryNum  = 0;
					entry.ParentNum     = iTable->DirectoryNum;
					entry.ParentExtent  = iTable->Extent;
					entry.Extent        = Extent;
					entry.IsRoot        = false;

					if ( PathTree.size() <= (i + 1) )
					{
						// Add a new level.
						PathTree.push_back( ISOPathTableEntries() );
					}

					PathTree[ i + 1 ].push_back( entry );

					Done = true;

					InsertedLevel = i + 1;

					break;
				}
			}

			if ( Done )
			{
				break;
			}
		}
	}

	// Now sort the level by parent num then name.
	std::sort( PathTree[ InsertedLevel ].begin(), PathTree[ InsertedLevel ].end(), PathTableSort );

	// Compile it back to a linear list ready for writing.
	CompilePathTable();
}

void ISOPathTable::RemoveDirectory( DWORD Extent )
{
	// Slightly easier, just remove the entry that has the matching Extent, then recompile.
	// We don't need to sort as the list will already be sorted.

	for ( int i = 0; i<PathTree.size(); i++ )
	{
		for ( ISOPathTableIterator iPath = PathTree[ i ].begin(); iPath != PathTree[ i ].end(); iPath++ )
		{
			if ( iPath->Extent == Extent )
			{
				// Got it. Remove it from the list, and recompile.
				(void) PathTree[ i ].erase( iPath );

				CompilePathTable();

				return;
			}
		}
	}
}

void ISOPathTable::BlankTable( DWORD RootExtent )
{
	TableEntries.clear();

	ISOPathTableEntry entry;

	entry.DirectoryNum = 1;
	entry.Extent       = RootExtent;
	entry.IsRoot       = true;
	entry.ParentExtent = RootExtent;
	entry.ParentNum    = 1;

	TableEntries.push_back( entry );

	CompilePathTree();
}

DWORD ISOPathTable::GetNumEntries()
{
	return (DWORD) TableEntries.size();
}

DWORD ISOPathTable::ExtentForDir( BYTEString &Identifier, DWORD ParentExtent )
{
	DWORD Extent = 0;

	for ( ISOPathTableIterator iPath = TableEntries.begin(); iPath != TableEntries.end(); iPath++ )
	{
		if ( rstrcmp( iPath->DirectoryName, Identifier ) )
		{
			if ( iPath->ParentExtent == ParentExtent )
			{
				Extent = iPath->Extent;
			}
		}
	}

	return Extent;
}