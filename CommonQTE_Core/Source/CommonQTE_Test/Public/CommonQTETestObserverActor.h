#pragma once

#include "CoreMinimal.h"
#include "CommonQTEObserverActor.h"
#include "CommonQTETestType.h"

#include "CommonQTETestObserverActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_TEST_API ACommonQTETestObserverActor : public ACommonQTEObserverActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void SetForcePresentationRoutingForTest(bool bInForcePresentationRouting);

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FCommonQTETestObserverTrace GetTestTrace() const;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void ResetTestTrace();

	virtual void SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot) override;
	virtual void PushStateDelta(const FCommonQTEObserverStateDelta& InStateDelta) override;

protected:
	virtual void BuildPresentationMessagesFromInitialState() override;
	virtual void BuildPresentationMessageFromStateDelta(const FCommonQTEObserverStateDelta& InStateDelta) override;
	virtual bool ShouldRoutePresentationMessages() const override;

private:
	UPROPERTY(BlueprintReadOnly, Category = "CommonQTE|Test", meta = (AllowPrivateAccess = "true"))
	FCommonQTETestObserverTrace TestTrace;

	UPROPERTY(EditAnywhere, Category = "CommonQTE|Test")
	bool bForcePresentationRoutingForTest = true;
};
