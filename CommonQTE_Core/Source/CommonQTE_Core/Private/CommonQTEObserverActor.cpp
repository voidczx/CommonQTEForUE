#include "CommonQTEObserverActor.h"

#include "CommonQTELog.h"
#include "CommonQTEManagerComponent.h"
#include "CommonQTEPresentationBuilder.h"
#include "CommonQTEStateRuntime.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ACommonQTEObserverActor::ACommonQTEObserverActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ACommonQTEObserverActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACommonQTEObserverActor, InitialStateSnapshot);
	DOREPLIFETIME(ACommonQTEObserverActor, ReplicatedState);
}

bool ACommonQTEObserverActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	for (const TWeakObjectPtr<APlayerController>& ExcludedPlayerController : ExcludedPlayerControllers)
	{
		const APlayerController* PlayerController = ExcludedPlayerController.Get();
		if (PlayerController == nullptr)
		{
			continue;
		}

		const APawn* PlayerPawn = PlayerController->GetPawn();
		if (RealViewer == PlayerController || ViewTarget == PlayerController || RealViewer == PlayerPawn || ViewTarget == PlayerPawn)
		{
			return false;
		}
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ACommonQTEObserverActor::SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot)
{
	InitialStateSnapshot = InInitialStateSnapshot;
	LastObservedStateSnapshot = InitialStateSnapshot;
	FCommonQTEStateRuntime::NormalizeStateSnapshot(LastObservedStateSnapshot);
	bHasLastObservedStateSnapshot = InitialStateSnapshot.WholeHandle.IsValid();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer initial state snapshot set. Handle=%d CellCount=%d Observer=%s"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), InitialStateSnapshot.CellContents.Num(), *GetNameSafe(this));
	if (ShouldRoutePresentationMessages())
	{
		BuildPresentationMessagesFromInitialState();
	}
	ReceiveCommonQTEPresentationStateChanged();
}

const FCommonQTEStateSnapshot& ACommonQTEObserverActor::GetInitialStateSnapshot() const
{
	return InitialStateSnapshot;
}

FCommonQTEHandle ACommonQTEObserverActor::GetCommonQTEHandle() const
{
	if (ReplicatedState.StateSnapshot.WholeHandle.IsValid())
	{
		return ReplicatedState.StateSnapshot.WholeHandle;
	}
	return InitialStateSnapshot.WholeHandle;
}

bool ACommonQTEObserverActor::HasCommonQTEPresentationStateSnapshot() const
{
	return InitialStateSnapshot.WholeHandle.IsValid() || ReplicatedState.StateSnapshot.WholeHandle.IsValid();
}

FCommonQTEStateSnapshot ACommonQTEObserverActor::GetCommonQTEPresentationStateSnapshot() const
{
	if (InitialStateSnapshot.WholeHandle.IsValid() && ReplicatedState.StateSnapshot.WholeHandle.IsValid())
	{
		FCommonQTEStateSnapshot StateSnapshot = FCommonQTEStateRuntime::ExpandCompactStateSnapshot(ReplicatedState.StateSnapshot, InitialStateSnapshot);
		FCommonQTEStateRuntime::NormalizeStateSnapshot(StateSnapshot);
		return StateSnapshot;
	}
	return InitialStateSnapshot;
}

void ACommonQTEObserverActor::SetExcludedPlayerControllers(const TArray<APlayerController*>& InPlayerControllers)
{
	ExcludedPlayerControllers.Reset();
	for (APlayerController* PlayerController : InPlayerControllers)
	{
		AddExcludedPlayerController(PlayerController);
	}
	ForceNetUpdate();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer excluded player controllers set. Count=%d Observer=%s"), ExcludedPlayerControllers.Num(), *GetNameSafe(this));
}

void ACommonQTEObserverActor::AddExcludedPlayerController(APlayerController* PlayerController)
{
	if (PlayerController != nullptr)
	{
		TWeakObjectPtr<APlayerController> WeakPlayerController;
		WeakPlayerController = PlayerController;
		ExcludedPlayerControllers.AddUnique(WeakPlayerController);
		ForceNetUpdate();
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer excluded player controller added. PlayerController=%s Observer=%s"), *GetNameSafe(PlayerController), *GetNameSafe(this));
	}
}

void ACommonQTEObserverActor::PushStateSnapshot(const FCommonQTEStateSnapshot& InStateSnapshot, const FCommonQTEObserverStateDelta& InStateDelta)
{
	ReplicatedState.StateSnapshot = FCommonQTEStateRuntime::BuildCompactStateSnapshot(InStateSnapshot);
	ReplicatedState.SequenceId = InStateDelta.SequenceId;
	ForceNetUpdate();
	PushStateDelta(InStateDelta);
}

void ACommonQTEObserverActor::PushStateDelta(const FCommonQTEObserverStateDelta& InStateDelta)
{
	StateDeltaBuffer.Add(InStateDelta);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer state delta pushed. Handle=%d SequenceId=%d CellIndex=%d BufferCount=%d Observer=%s"), FCommonQTELog::HandleValue(InStateDelta.WholeHandle), InStateDelta.SequenceId, InStateDelta.CellIndex, StateDeltaBuffer.Num(), *GetNameSafe(this));
	if (StateDeltaBuffer.Num() > MaxStateDeltaBufferCount)
	{
		StateDeltaBuffer.RemoveAt(0, StateDeltaBuffer.Num() - MaxStateDeltaBufferCount, EAllowShrinking::No);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer state delta buffer trimmed. BufferCount=%d Observer=%s"), StateDeltaBuffer.Num(), *GetNameSafe(this));
	}

	if (ShouldRoutePresentationMessages() && InStateDelta.SequenceId > LastConsumedSequenceId)
	{
		BuildPresentationMessageFromStateDelta(InStateDelta);
		LastConsumedSequenceId = InStateDelta.SequenceId;
	}
}

const TArray<FCommonQTEObserverStateDelta>& ACommonQTEObserverActor::GetStateDeltaBuffer() const
{
	return StateDeltaBuffer;
}

void ACommonQTEObserverActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer EndPlay. Observer=%s StateDeltaCount=%d"), *GetNameSafe(this), StateDeltaBuffer.Num());
	StateDeltaBuffer.Reset();
	LastConsumedSequenceId = 0;
	bHasLastObservedStateSnapshot = false;

	Super::EndPlay(EndPlayReason);
}

void ACommonQTEObserverActor::OnRep_InitialState()
{
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer initial state replicated. Handle=%d Observer=%s"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), *GetNameSafe(this));
	LastObservedStateSnapshot = InitialStateSnapshot;
	FCommonQTEStateRuntime::NormalizeStateSnapshot(LastObservedStateSnapshot);
	bHasLastObservedStateSnapshot = InitialStateSnapshot.WholeHandle.IsValid();
	BuildPresentationMessagesFromInitialState();
	BuildPresentationMessagesFromReplicatedState();
	ReceiveCommonQTEPresentationStateChanged();
}

void ACommonQTEObserverActor::OnRep_ReplicatedState()
{
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer replicated state received. Handle=%d SequenceId=%d LastConsumedSequenceId=%d Observer=%s"), FCommonQTELog::HandleValue(ReplicatedState.StateSnapshot.WholeHandle), ReplicatedState.SequenceId, LastConsumedSequenceId, *GetNameSafe(this));
	BuildPresentationMessagesFromReplicatedState();
	ReceiveCommonQTEPresentationStateChanged();
}

void ACommonQTEObserverActor::BuildPresentationMessagesFromInitialState()
{
	UCommonQTEManagerComponent* Manager = FindManager();
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Observer failed to route initial presentation messages because manager is missing. Handle=%d Observer=%s"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), *GetNameSafe(this));
		return;
	}

	TArray<FCommonQTEPresentationMessage> Messages;
	FCommonQTEPresentationBuilder::BuildInitialMessages(InitialStateSnapshot, ECommonQTEPresentationSource::Observer, Messages);
	Manager->EnqueuePresentationMessages(Messages);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer initial presentation messages routed. Handle=%d Count=%d Observer=%s"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), Messages.Num(), *GetNameSafe(this));
}

void ACommonQTEObserverActor::BuildPresentationMessageFromStateDelta(const FCommonQTEObserverStateDelta& InStateDelta)
{
	FCommonQTEPresentationMessage Message;
	FCommonQTEPresentationBuilder::BuildStateDeltaMessage(InStateDelta, ECommonQTEPresentationSource::Observer, Message);

	if (UCommonQTEManagerComponent* Manager = FindManager())
	{
		Manager->EnqueuePresentationMessage(Message);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer delta presentation message routed. Handle=%d SequenceId=%d CellIndex=%d Observer=%s"), FCommonQTELog::HandleValue(InStateDelta.WholeHandle), InStateDelta.SequenceId, InStateDelta.CellIndex, *GetNameSafe(this));
	}
	else
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Observer failed to route delta presentation message because manager is missing. Handle=%d SequenceId=%d Observer=%s"), FCommonQTELog::HandleValue(InStateDelta.WholeHandle), InStateDelta.SequenceId, *GetNameSafe(this));
	}
}

void ACommonQTEObserverActor::BuildPresentationMessagesFromReplicatedState()
{
	if (!ReplicatedState.StateSnapshot.WholeHandle.IsValid() || ReplicatedState.SequenceId <= LastConsumedSequenceId)
	{
		return;
	}

	if (!InitialStateSnapshot.WholeHandle.IsValid())
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Observer delayed replicated state routing because initial state is missing. Handle=%d SequenceId=%d Observer=%s"), FCommonQTELog::HandleValue(ReplicatedState.StateSnapshot.WholeHandle), ReplicatedState.SequenceId, *GetNameSafe(this));
		return;
	}

	const FCommonQTEStateSnapshot StateBeforeReplication = bHasLastObservedStateSnapshot ? LastObservedStateSnapshot : InitialStateSnapshot;
	FCommonQTEStateSnapshot StateAfterReplication = FCommonQTEStateRuntime::ExpandCompactStateSnapshot(ReplicatedState.StateSnapshot, InitialStateSnapshot);

	TArray<FCommonQTEPresentationMessage> Messages;
	FCommonQTEPresentationBuilder::BuildSnapshotDiffMessages(StateBeforeReplication, StateAfterReplication, ECommonQTEPresentationSource::Observer, ECommonQTEPresentationPhase::StateDelta, 0, Messages);
	for (FCommonQTEPresentationMessage& Message : Messages)
	{
		Message.SequenceId = ReplicatedState.SequenceId;
	}

	LastObservedStateSnapshot = StateAfterReplication;
	bHasLastObservedStateSnapshot = true;
	LastConsumedSequenceId = ReplicatedState.SequenceId;

	if (Messages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer replicated state produced no presentation diff. Handle=%d SequenceId=%d Observer=%s"), FCommonQTELog::HandleValue(ReplicatedState.StateSnapshot.WholeHandle), ReplicatedState.SequenceId, *GetNameSafe(this));
		return;
	}

	if (UCommonQTEManagerComponent* Manager = FindManager())
	{
		Manager->EnqueuePresentationMessages(Messages);
		UE_LOG(LogCommonQTE, Verbose, TEXT("Observer replicated state presentation messages routed. Handle=%d SequenceId=%d MessageCount=%d Observer=%s"), FCommonQTELog::HandleValue(ReplicatedState.StateSnapshot.WholeHandle), ReplicatedState.SequenceId, Messages.Num(), *GetNameSafe(this));
	}
	else
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Observer failed to route replicated state messages because manager is missing. Handle=%d SequenceId=%d Observer=%s"), FCommonQTELog::HandleValue(ReplicatedState.StateSnapshot.WholeHandle), ReplicatedState.SequenceId, *GetNameSafe(this));
	}
}

bool ACommonQTEObserverActor::ShouldRoutePresentationMessages() const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer presentation routing skipped on dedicated server. Observer=%s"), *GetNameSafe(this));
		return false;
	}

	if (!HasAuthority())
	{
		return true;
	}

	for (const TWeakObjectPtr<APlayerController>& ExcludedPlayerController : ExcludedPlayerControllers)
	{
		const APlayerController* PlayerController = ExcludedPlayerController.Get();
		if (PlayerController != nullptr && PlayerController->NetConnection == nullptr)
		{
			UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Observer presentation routing skipped because local player is excluded. Observer=%s PlayerController=%s"), *GetNameSafe(this), *GetNameSafe(PlayerController));
			return false;
		}
	}

	return true;
}

UCommonQTEManagerComponent* ACommonQTEObserverActor::FindManager() const
{
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->FindComponentByClass<UCommonQTEManagerComponent>() : nullptr;
}
