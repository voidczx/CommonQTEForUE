#pragma once

#include "CoreMinimal.h"
#include "CommonQTEActorClassConfig.h"
#include "CommonQTEHandle.h"
#include "CommonQTEType.h"
#include "Components/GameStateComponent.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"

#include "CommonQTEManagerComponent.generated.h"

class FCommonQTEEntity;
class FCommonQTECellGeneratePolicy;
class AActor;
class ACommonQTEObserverActor;
class ACommonQTEPerformerActor;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCommonQTEPerformerVerificationApplied, ACommonQTEPerformerActor*, Performer, const FCommonQTEPerformerPredictionMessage&, Message, const FCommonQTEApplyInputResult&, ApplyResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonQTEEntityFinalized, FCommonQTEHandle, Handle, ECommonQTEState, State);

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_CORE_API UCommonQTEManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UCommonQTEManagerComponent(const FObjectInitializer& ObjectInitializer);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE")
	FCommonQTEPresentationMessagesReady OnPresentationMessagesReady;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE")
	FCommonQTEPerformerVerificationApplied OnPerformerVerificationApplied;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE")
	FCommonQTEEntityFinalized OnEntityFinalized;

	FCommonQTEHandle CreateEntity();
	FCommonQTEHandle CreateEntity(const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig);
	FCommonQTEHandle CreateEntity(const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const FCommonQTEActorClassConfig& ActorClassConfig);
	FCommonQTEHandle CreateEntityWithFixedCells(const FCommonQTERule& Rule, const TArray<int32>& CellContents);
	FCommonQTEHandle CreateEntityWithFixedCells(const FCommonQTERule& Rule, const TArray<int32>& CellContents, const FCommonQTEActorClassConfig& ActorClassConfig);
	FCommonQTEHandle CreateEntityWithPolicy(const FCommonQTERule& Rule, const TSharedPtr<FCommonQTECellGeneratePolicy>& CellGeneratePolicy, const FCommonQTEActorClassConfig& ActorClassConfig);
	bool RemoveEntity(const FCommonQTEHandle& Handle);
	bool StopEntity(const FCommonQTEHandle& Handle);
	bool PauseEntity(const FCommonQTEHandle& Handle);
	bool ResumeEntity(const FCommonQTEHandle& Handle);
	int32 PauseAllEntities();
	int32 ResumeAllEntities();
	void Reset();

	ACommonQTEPerformerActor* AddPerformer(const FCommonQTEHandle& Handle, AActor* PerformerOwner);
	ACommonQTEObserverActor* CreateObserver(const FCommonQTEHandle& Handle);
	TArray<ACommonQTEPerformerActor*> GetPerformers(const FCommonQTEHandle& Handle) const;
	ACommonQTEObserverActor* GetObserver(const FCommonQTEHandle& Handle) const;

	ECommonQTEState GetEntityState(const FCommonQTEHandle& Handle) const;
	ECommonQTECellState GetCellState(const FCommonQTEHandle& Handle, int32 CellIndex) const;
	bool GetEntityStateSnapshot(const FCommonQTEHandle& Handle, FCommonQTEStateSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void SetEntityFinishedCleanupDelay(FCommonQTEHandle Handle, float CleanupDelay);

	UFUNCTION(BlueprintPure, Category = "CommonQTE")
	float GetEntityFinishedCleanupDelay(FCommonQTEHandle Handle) const;

	void RequestVerificationDrain(ACommonQTEPerformerActor* Performer);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void EnqueuePresentationMessage(const FCommonQTEPresentationMessage& Message);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void EnqueuePresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages);

	TArray<FCommonQTEPresentationMessage> ConsumePresentationMessages();

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	TArray<FCommonQTEPresentationMessage> ConsumePresentationMessages(int32 MaxMessages);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void SetPresentationDrainConfig(FCommonQTEPresentationDrainConfig InConfig);

	UFUNCTION(BlueprintPure, Category = "CommonQTE")
	FCommonQTEPresentationDrainConfig GetPresentationDrainConfig() const;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void SetMaxPresentationMessagesPerDrain(int32 InMaxMessagesPerDrain);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void SetPresentationDrainInterval(float InDrainInterval);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	void SetAutoDrainPresentationMessages(bool bInAutoDrain);

private:
	TSharedPtr<FCommonQTEEntity> FindEntity(const FCommonQTEHandle& Handle) const;
	FCommonQTEActorClassConfig ResolveActorClassConfig(const FCommonQTEActorClassConfig& ActorClassConfig) const;
	TSubclassOf<ACommonQTEPerformerActor> ResolvePerformerActorClass(TSubclassOf<ACommonQTEPerformerActor> RuntimeActorClass) const;
	TSubclassOf<ACommonQTEObserverActor> ResolveObserverActorClass(TSubclassOf<ACommonQTEObserverActor> RuntimeActorClass) const;
	FCommonQTEActorClassConfig GetEntityActorClassConfig(const FCommonQTEHandle& Handle) const;
	bool IsAuthority() const;
	void ScheduleVerificationDrain();
	void DrainVerificationQueues();
	void SchedulePresentationDrain();
	void DrainPresentationMessages();
	TArray<FCommonQTEPresentationMessage> ConsumePresentationMessagesForDrain();
	TArray<FCommonQTEPresentationMessage> ConsumeInitialPresentationBatch();
	bool IsInitialPresentationMessage(const FCommonQTEPresentationMessage& Message) const;
	bool IsSameInitialPresentationBatch(const FCommonQTEPresentationMessage& Left, const FCommonQTEPresentationMessage& Right) const;
	bool IsPresentationMessageQueueEmpty() const;
	int32 GetPresentationMessageQueueCount() const;
	const FCommonQTEPresentationMessage* PeekPresentationMessage() const;
	void CompactPresentationMessageQueueIfNeeded();
	void HandlePerformerVerification(ACommonQTEPerformerActor* Performer, const FCommonQTEPerformerPredictionMessage& Message);
	bool IsPerformerRegisteredForEntity(const TSharedPtr<FCommonQTEEntity>& Entity, ACommonQTEPerformerActor* Performer) const;
	bool CanVerifyPredictionMessage(ACommonQTEPerformerActor* Performer, const FCommonQTEEntity& Entity, const FCommonQTEPerformerPredictionMessage& Message) const;
	void MarkPredictionMessageResolved(ACommonQTEPerformerActor* Performer, int32 PredictionId);
	void EnqueueRollBackToPerformer(ACommonQTEPerformerActor* Performer, const FCommonQTEHandle& Handle, int32 PredictionId, int32 RejectedCellIndex, const FCommonQTEStateSnapshot& AuthoritativeStateSnapshot);
	void UpdateInitialStateSnapshot(const FCommonQTEHandle& Handle);
	void UpdateObserverExcludedPlayerControllers(const FCommonQTEHandle& Handle);
	void UpdateObserverStateDelta(const FCommonQTEHandle& Handle, int32 ChangedCellIndex);
	void SyncPerformersToAuthoritativeState(const FCommonQTEHandle& Handle, int32 PredictionId, int32 RejectedCellIndex);
	bool IsTerminalState(ECommonQTEState State) const;
	void FinalizeEntity(const FCommonQTEHandle& Handle);
	void ScheduleEntityCleanup(const FCommonQTEHandle& Handle);
	void HandleCleanupTimerExpired(FCommonQTEHandle Handle);
	void HandleLimitTimeExpired(FCommonQTEHandle Handle);
	void ClearLimitTimer(const FCommonQTEHandle& Handle);
	void ClearCleanupTimer(const FCommonQTEHandle& Handle);
	int32 AllocateObserverSequenceId(const FCommonQTEHandle& Handle);
	APlayerController* ResolvePlayerController(AActor* PerformerOwner) const;
	void CheckPresentationMessageQueueThreshold(const TCHAR* Context);
	float ResolveEntityFinishedCleanupDelay(const FCommonQTEHandle& Handle) const;

	UPROPERTY(EditDefaultsOnly, Category = "CommonQTE")
	float FinishedEntityCleanupDelay = 2.0f;

	TMap<FCommonQTEHandle, TSharedPtr<FCommonQTEEntity>> EntityMap;
	TMap<FCommonQTEHandle, FTimerHandle> EntityTimers;
	TMap<FCommonQTEHandle, FTimerHandle> EntityCleanupTimers;
	TMap<FCommonQTEHandle, float> EntityFinishedCleanupDelays;
	UPROPERTY(Transient)
	TMap<FCommonQTEHandle, FCommonQTEActorClassConfig> EntityActorClassConfigMap;
	TSet<FCommonQTEHandle> FinalizedEntities;
	TMap<FCommonQTEHandle, int32> ObserverSequenceIdMap;
	TArray<TWeakObjectPtr<ACommonQTEPerformerActor>> DirtyVerificationPerformers;
	FTimerHandle VerificationDrainTimerHandle;
	FTimerHandle PresentationDrainTimerHandle;
	FCommonQTEPresentationDrainConfig PresentationDrainConfig;
	TArray<FCommonQTEPresentationMessage> PresentationMessageQueue;
	int32 PresentationMessageQueueHead = 0;
	TMap<TObjectKey<ACommonQTEPerformerActor>, int32> LastResolvedPredictionIdMap;
	bool bVerificationDrainScheduled = false;
	bool bPresentationDrainScheduled = false;
	bool bPresentationQueueThresholdReported = false;
	int32 NextHandleValue = 1;
};
