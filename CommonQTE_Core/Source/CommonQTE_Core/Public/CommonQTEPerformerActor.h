#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "CommonQTEPerformerActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonQTEPerformerPredictionMessagesReady, const TArray<FCommonQTEPerformerPredictionMessage>&, Messages);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonQTEPerformerRollBackMessagesReady, const TArray<FCommonQTEPerformerRollBackMessage>&, Messages);

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_CORE_API ACommonQTEPerformerActor : public AActor
{
	GENERATED_BODY()

public:
	ACommonQTEPerformerActor();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE")
	FCommonQTEPerformerPredictionMessagesReady OnLocalPredictionMessagesReady;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE")
	FCommonQTEPerformerRollBackMessagesReady OnRollBackMessagesReady;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	virtual FCommonQTEPerformerPredictionMessage SubmitPrediction(int32 CellContent);

	virtual void SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot);
	const FCommonQTEStateSnapshot& GetInitialStateSnapshot() const;
	const FCommonQTEStateSnapshot& GetPredictedStateSnapshot() const;
	virtual void EnqueuePredictionMessage(const FCommonQTEPerformerPredictionMessage& Message);
	virtual void EnqueueRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message);
	virtual void FlushPredictionMessagesToServer();
	virtual void FlushRollBackMessagesToClient();
	virtual void AcknowledgePredictionMessage(int32 PredictionId);

	virtual TArray<FCommonQTEPerformerPredictionMessage> ConsumeLocalPredictionMessages();
	virtual TArray<FCommonQTEPerformerPredictionMessage> ConsumePendingVerificationMessages();
	virtual TArray<FCommonQTEPerformerRollBackMessage> ConsumeRollBackMessages();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void SendPredictionMessagesToServer(const TArray<FCommonQTEPerformerPredictionMessage>& Messages);

	UFUNCTION(Client, Reliable)
	void SendRollBackMessagesToClient(const TArray<FCommonQTEPerformerRollBackMessage>& Messages);

	UFUNCTION(Client, Reliable)
	void SendPredictionAckToClient(int32 PredictionId);

	UFUNCTION()
	virtual void OnRep_InitialStateSnapshot();

	virtual void ReceivePredictionMessages(const TArray<FCommonQTEPerformerPredictionMessage>& Messages);
	virtual void ReceiveRollBackMessages(const TArray<FCommonQTEPerformerRollBackMessage>& Messages);
	virtual void ScheduleLocalPredictionDrain();
	virtual void ScheduleRollBackDrain();
	virtual void DrainLocalPredictionMessages();
	virtual void DrainRollBackMessages();
	virtual void ResetPredictedStateFromInitialState();
	virtual void RouteInitialStatePresentationMessages();
	virtual void RoutePredictionPresentationMessages(const TArray<FCommonQTEPerformerPredictionRecord>& PredictionRecordsToRoute);
	virtual void RouteRollBackPresentationMessages(const FCommonQTEPerformerRollBackMessage& Message, const FCommonQTEStateSnapshot& StateBeforeRollBack, const FCommonQTEStateSnapshot& StateAfterRollBack);
	virtual FCommonQTEPerformerPredictionMessage BuildPredictionMessage(int32 CellContent);
	virtual FCommonQTEPerformerPredictionMessage NormalizePredictionMessage(const FCommonQTEPerformerPredictionMessage& Message);
	virtual bool ApplyPredictionMessage(const FCommonQTEPerformerPredictionMessage& Message, FCommonQTEPerformerPredictionRecord& OutRecord);
	virtual bool ApplyRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message, FCommonQTEStateSnapshot& OutStateBeforeRollBack, FCommonQTEStateSnapshot& OutStateAfterRollBack);
	virtual void PrunePredictionRecordsUpTo(int32 PredictionId);
	virtual bool ShouldRoutePresentationMessages() const;
	virtual class UCommonQTEManagerComponent* FindManager() const;

	UPROPERTY(ReplicatedUsing = OnRep_InitialStateSnapshot)
	FCommonQTEStateSnapshot InitialStateSnapshot;

	FCommonQTEStateSnapshot PredictedStateSnapshot;
	TArray<FCommonQTEPerformerPredictionRecord> PredictionRecords;
	TArray<FCommonQTEPerformerPredictionMessage> LocalPredictionMessageQueue;
	TArray<FCommonQTEPerformerPredictionMessage> OutboundPredictionMessageQueue;
	TArray<FCommonQTEPerformerPredictionMessage> PendingVerificationMessageQueue;
	TArray<FCommonQTEPerformerRollBackMessage> OutboundRollBackMessageQueue;
	TArray<FCommonQTEPerformerRollBackMessage> RollBackMessageQueue;
	FTimerHandle LocalPredictionDrainTimerHandle;
	FTimerHandle RollBackDrainTimerHandle;
	int32 LastAppliedRollBackPredictionId = 0;
	bool bLocalPredictionDrainScheduled = false;
	bool bRollBackDrainScheduled = false;
	bool bHasInitialStateSnapshot = false;
	int32 NextPredictionId = 1;
};
