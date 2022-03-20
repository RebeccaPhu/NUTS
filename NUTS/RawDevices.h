#pragma once

#include "stdafx.h"

#include <vector>
#include <string>

typedef std::vector<std::wstring> RawPaths;

BYTEString ReadDeviceProductID( BYTE drive );
RawPaths GetRawDevices( );