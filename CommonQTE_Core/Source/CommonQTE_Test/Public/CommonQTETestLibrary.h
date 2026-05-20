#pragma once

#include "CoreMinimal.h"
#include "CommonQTETestType.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CommonQTETestLibrary.generated.h"

class AActor;
class UCommonQTEFlowTestRunner;

UCLASS()
class COMMONQTE_TEST_API UCommonQTETestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test", meta = (WorldContext = "WorldContextObject"))
	static UCommonQTEFlowTestRunner* StartCommonQTETest(UObject* WorldContextObject, AActor* PerformerOwner, FCommonQTETestConfig Config);
};
