#include "CommonQTETestLibrary.h"

#include "CommonQTEFlowTestRunner.h"

UCommonQTEFlowTestRunner* UCommonQTETestLibrary::StartCommonQTETest(UObject* WorldContextObject, AActor* PerformerOwner, FCommonQTETestConfig Config)
{
	UCommonQTEFlowTestRunner* Runner = NewObject<UCommonQTEFlowTestRunner>(GetTransientPackage());
	if (Runner != nullptr)
	{
		Runner->StartTest(WorldContextObject, PerformerOwner, Config);
	}
	return Runner;
}
