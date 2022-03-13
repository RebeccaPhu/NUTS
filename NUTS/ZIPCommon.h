#pragma once

#include "zip.h"
#include "DataSource.h"

#include "TempFile.h"

#include <vector>

extern const FSIdentifier FSID_ZIP;

class ZIPCommon
{
public:
	ZIPCommon(void);
	~ZIPCommon(void);

public:
	static zip_uint64_t _NUTSZipCallback( void *userdata, void *data, zip_uint64_t len, zip_source_cmd_t cmd )
	{
		ZIPCommon *zipHandler = (ZIPCommon *) userdata;

		return zipHandler->NUTSZipCallback( data, len, cmd );
	}
	
	zip_uint64_t NUTSZipCallback( void *data, zip_uint64_t len, zip_source_cmd_t cmd );

	virtual DataSource *GetSource() = 0;

	zip_uint64_t ReadPtr;
	zip_uint64_t WritePtr;

private:
	void NixClone()
	{
		if ( pWriteClone != nullptr )
		{
			/* This wipes out the temp file */
			CTempFile Fudger( pWriteClone->Name() );

			delete pWriteClone;
		}

		pWriteClone = nullptr;
	}

	CTempFile *pWriteClone;
};

