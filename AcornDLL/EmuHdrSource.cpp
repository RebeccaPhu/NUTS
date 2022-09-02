#include "StdAfx.h"
#include "EmuHdrSource.h"

EmuHdrSource::EmuHdrSource( DataSource *pSource ) : OffsetDataSource( 0x200, pSource )
{
}


EmuHdrSource::~EmuHdrSource(void)
{
}
