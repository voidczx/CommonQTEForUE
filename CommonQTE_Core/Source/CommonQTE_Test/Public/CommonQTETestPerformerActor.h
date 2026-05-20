#pragma once

#include "CoreMinimal.h"
#include "CommonQTEPerformerActor.h"
#include "CommonQTETestType.h"

#include "CommonQTETestPerformerActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_TEST_API ACommonQTETestPerformerActor : public ACommonQTEPerformerActor
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void SetNetworkSimulationConfig(const FCommonQTETestNetworkSimulationConfig& InConfig);

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FCommonQTETestNetworkSimulationConfig GetNetworkSimulationConfig() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	int32 GetPendingDelayedPredictionMessageCount() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	int32 GetPendingDelayedRollBackMessageCount() const;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	FCommonQTEPerformerPredictionMessage SubmitPredictionForTest(int32 LocalCellContent, int32 OutboundCellContent);

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FCommonQTETestPerformerTrace GetTestTrace() const;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void ResetTestTrace();

	virtual void SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot) override;
	virtual void EnqueueRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message) override;
	virtual void FlushPredictionMessagesToServer() override;

protected:
	virtual void DrainLocalPredictionMessages() override;
	virtual void DrainRollBackMessages() override;
	virtual void RoutePredictionPresentationMessages(const TArray<FCommonQTEPerformerPredictionRecord>& PredictionRecordsToRoute) override;
	virtual void RouteRollBackPresentationMessages(const FCommonQTEPerformerRollBackMessage& Message, const FCommonQTEStateSnapshot& StateBeforeRollBack, const FCommonQTEStateSnapshot& StateAfterRollBack) override;
	virtual bool ApplyPredictionMessage(const FCommonQTEPerformerPredictionMessage& Message, FCommonQTEPerformerPredictionRecord& OutRecord) override;
	virtual bool ApplyRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message, FCommonQTEStateSnapshot& OutStateBeforeRollBack, FCommonQTEStateSnapshot& OutStateAfterRollBack) override;

private:
	bool IsNetworkSimulationEnabled() const;
	bool IsListenServerHostPerformer() const;
	bool ShouldDelayPredictionToAuthority() const;
	bool ShouldDelayRollBackToPerformer() const;
	void DeliverDelayedPredictionMessages(TArray<FCommonQTEPerformerPredictionMessage> Messages);
	void DeliverDelayedRollBackMessage(FCommonQTEPerformerRollBackMessage Message);
	void ClearNetworkSimulationTimers();

	UPROPERTY(BlueprintReadOnly, Category = "CommonQTE|Test", meta = (AllowPrivateAccess = "true"))
	FCommonQTETestPerformerTrace TestTrace;

	UPROPERTY(EditAnywhere, Category = "CommonQTE|Test")
	FCommonQTETestNetworkSimulationConfig NetworkSimulationConfig;

	TArray<FTimerHandle> NetworkSimulationTimerHandles;
	int32 PendingDelayedPredictionMessageCount = 0;
	int32 PendingDelayedRollBackMessageCount = 0;
};
