#include "stdafx.h"

#include "DMS.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/TempFile.h"
#include "../NUTS/SourceFunctions.h"
#include "../NUTS/libfuncs.h"

#include "xDMS/pfile.h"

std::wstring ErrMsg( USHORT err ){
	switch (err) {
		case NO_PROBLEM:
		case FILE_END:
			return L"Success";
		case ERR_NOMEMORY:
			return L"Not enough memory for buffers";
		case ERR_CANTOPENIN:
			return L"Can't read from input store";
		case ERR_CANTOPENOUT:
			return L"Can't write to output store";
		case ERR_NOTDMS:
			return L"Image is not a DMS archive";
		case ERR_SREAD:
			return L"Unexpected end of file";
		case ERR_HCRC:
			return L"Header CRC error";
		case ERR_NOTTRACK:
			return L"Track header not found";
		case ERR_BIGTRACK:
			return L"Track too big";
		case ERR_THCRC:
			return L"Track header CRC error";
		case ERR_TDCRC:
			return L"Track data CRC error";
		case ERR_CSUM:
			return L"Checksum error after unpacking";
		case ERR_CANTWRITE:
			return L"Can't write to output store";
		case ERR_BADDECR:
			return L"Error unpacking";
		case ERR_UNKNMODE:
			return L"Unknown compression mode used";
		case ERR_NOPASSWD:
			return L"File is encrypted";
		case ERR_BADPASSWD:
			return L"The password is probably wrong.";
		case ERR_FMS:
			return L"Image is not really a compressed disk image, but an FMS archive";
		default:
			return L"Internal error while processing image";
	}
}

int ExtractDMS( DataSource *pSource, CTempFile &nestObj )
{
	/* First extract the source to a temp file so that xDMS can get at it */
	CTempFile srcObj;

	SourceToTemp( pSource, srcObj );

	std::string srcPath = AString( (WCHAR *) srcObj.Name().c_str() );
	std::string dPath   = AString( (WCHAR *) nestObj.Name().c_str() );

	USHORT ret = Process_File(
		(char *) srcPath.c_str(),
		(char *) dPath.c_str(),
		CMD_UNPACK,
		OPT_QUIET,
		0,
		0
	);

	if ( ret == NO_PROBLEM )
	{
		return DS_SUCCESS;
	}

	return NUTSError( 0x300, ErrMsg( ret ) );
}
