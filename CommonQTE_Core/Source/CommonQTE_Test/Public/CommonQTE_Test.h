#pragma once

#include "Modules/ModuleManager.h"

class FCommonQTE_TestModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
