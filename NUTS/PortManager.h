#pragma once

#include <vector>

#include "NUTSTypes.h"

typedef enum _DevicePortType
{
	PortTypeSerial   = 1,
	PortTypeParallel = 2,
} DevicePortType;

typedef struct _DevicePort
{
	WORD           Index;
	DevicePortType Type;
	std::wstring   Name;
} DevicePort;

typedef struct _ConfiguredDevicePort
{
	WORD             Index;
	DevicePortType   Type;
	PluginIdentifier PLID;
	std::wstring     Name;
} ConfiguredDevicePort;

class PortManager
{
public:
	PortManager(void);
	~PortManager(void);

public:
	void ConfigurePorts( void );

	std::vector<DevicePort> PortManager::EnumeratePorts( void );

	void ReadConfiguration( void );
	void StoreConfiguration( void );

	std::vector<ConfiguredDevicePort> GetConfiguration( void ) const { return Config; }

	void AddPort( ConfiguredDevicePort dp )
	{
		Config.push_back( dp );
	};

	void DeletePort( int i )
	{
		Config.erase( Config.begin() + i );
	}

private:
	std::vector<ConfiguredDevicePort> Config;
};

extern PortManager PortConfig;