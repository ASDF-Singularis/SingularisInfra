#pragma once

#include <Modules/ModuleManager.h>

class FSingularisInfraModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
