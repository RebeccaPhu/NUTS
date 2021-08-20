#pragma once

#include <string>
#include <vector>

typedef struct _BuiltInTranslator
{
	DWORD TUID;
	DWORD TXFlags;
	std::wstring FriendlyName;
} BuiltInTranslator;

typedef std::vector<BuiltInTranslator> BuiltInTranslators;
typedef std::vector<BuiltInTranslator>::iterator BuiltInTranslator_iter;

class BuiltIns
{
public:
	BuiltIns(void);
	~BuiltIns(void);

	BuiltInTranslators GetBuiltInTranslators( void );

	void *LoadTranslator( DWORD TUID );
};

extern BuiltIns NUTSBuiltIns;

