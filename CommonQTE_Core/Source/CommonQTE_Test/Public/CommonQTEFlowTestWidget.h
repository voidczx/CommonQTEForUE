#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommonQTETestType.h"

#include "CommonQTEFlowTestWidget.generated.h"

class UCommonQTEFlowTestRunner;

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_TEST_API UCommonQTEFlowTestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void InitializeWithRunner(UCommonQTEFlowTestRunner* InRunner);

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	UCommonQTEFlowTestRunner* GetRunner() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "CommonQTE|Test")
	void OnTestReportUpdated(const FCommonQTETestReport& Report);

private:
	UPROPERTY(BlueprintReadOnly, Category = "CommonQTE|Test", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCommonQTEFlowTestRunner> Runner;
};
