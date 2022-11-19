#include "StdAfx.h"
#include "EmuHdrSource.h"

EmuHdrSource::EmuHdrSource( DataSource *pSource ) : OffsetDataSource( 0x200, pSource )
{
	Feedback = L"RISC OS Emulation Header";
}


EmuHdrSource::~EmuHdrSource(void)
{
}
