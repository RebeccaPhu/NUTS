#pragma once

#include "../NUTS/TempFile.h"
#include "../NUTS/Directory.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/libfuncs.h"

#include <map>

#include "IconResolve.h"

class ADFSCommon
{
public:
	ADFSCommon(void);
	~ADFSCommon(void);

protected:
	int  ExportSidecar( NativeFile *pFile, SidecarExport &sidecar );
	int  ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj );

	int  RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory, bool AllowLongNames );

	void FreeAppIcons( Directory *pDirectory );

	NativeFile OverrideFile;

	bool Override;

	bool CommonUseResolvedIcons;

	void InterpretImportedType( NativeFile *pFile );

	bool CloneWars;
	
public:
	void InterpretNativeType( NativeFile *pFile );
	void SetTimeStamp( NativeFile *pFile );
};

