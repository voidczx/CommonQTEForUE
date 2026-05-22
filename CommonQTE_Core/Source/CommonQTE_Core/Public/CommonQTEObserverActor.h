#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"
#include "GameFramework/Actor.h"

#include "CommonQTEObserverActor.generated.h"

class APlayerController;
class UCommonQTEManagerComponent;

UCLASS(BlueprintType, Blueprintable)
class COMMONQTE_CORE_API ACommonQTEObserverActor : public AActor
{
	GENERATED_BODY()

public:
	ACommonQTEObserverActor();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	virtual void SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot);
	const FCommonQTEStateSnapshot& GetInitialStateSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE")
	FCommonQTEHandle GetCommonQTEHandle() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE")
	bool HasCommonQTEPresentationStateSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE")
	FCommonQTEStateSnapshot GetCommonQTEPresentationStateSnapshot() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "CommonQTE")
	void ReceiveCommonQTEPresentationStateChanged();

	virtual void SetExcludedPlayerControllers(const TArray<APlayerController*>& InPlayerControllers);
	virtual void AddExcludedPlayerController(APlayerController* PlayerController);

	virtual void PushStateSnapshot(const FCommonQTEStateSnapshot& InStateSnapshot, const FCommonQTEObserverStateDelta& InStateDelta);
	virtual void PushStateDelta(const FCommonQTEObserverStateDelta& InStateDelta);
	const TArray<FCommonQTEObserverStateDelta>& GetStateDeltaBuffer() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	virtual void OnRep_InitialState();

	UFUNCTION()
	virtual void OnRep_ReplicatedState();

	virtual void BuildPresentationMessagesFromInitialState();
	virtual void BuildPresentationMessageFromStateDelta(const FCommonQTEObserverStateDelta& InStateDelta);
	virtual void BuildPresentationMessagesFromReplicatedState();
	virtual void NotifyCommonQTEPresentationStateChangedIfReady();
	virtual bool ShouldRoutePresentationMessages() const;
	virtual UCommonQTEManagerComponent* FindManager() const;

	UPROPERTY(ReplicatedUsing = OnRep_InitialState)
	FCommonQTEStateSnapshot InitialStateSnapshot;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedState)
	FCommonQTEObserverReplicatedState ReplicatedState;

	TArray<TWeakObjectPtr<APlayerController>> ExcludedPlayerControllers;
	TArray<FCommonQTEObserverStateDelta> StateDeltaBuffer;
	FCommonQTEStateSnapshot LastObservedStateSnapshot;
	int32 MaxStateDeltaBufferCount = 32;
	int32 LastConsumedSequenceId = 0;
	bool bHasLastObservedStateSnapshot = false;
};
