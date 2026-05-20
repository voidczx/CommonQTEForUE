#include "CommonQTE_Core.h"

#include "CommonQTELog.h"

void FCommonQTE_CoreModule::StartupModule()
{
	UE_LOG(LogCommonQTE, Log, TEXT("CommonQTE_Core module started."));
}

void FCommonQTE_CoreModule::ShutdownModule()
{
	UE_LOG(LogCommonQTE, Log, TEXT("CommonQTE_Core module shut down."));
}

IMPLEMENT_MODULE(FCommonQTE_CoreModule, CommonQTE_Core)
