#include "CommonQTEPerformerActor.h"

#include "CommonQTELog.h"
#include "CommonQTEManagerComponent.h"
#include "CommonQTEPresentationBuilder.h"
#include "CommonQTEStateRuntime.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ACommonQTEPerformerActor::ACommonQTEPerformerActor()
{
	bReplicates = true;
}

void ACommonQTEPerformerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACommonQTEPerformerActor, InitialStateSnapshot);
}

void ACommonQTEPerformerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalPredictionDrainTimerHandle);
		World->GetTimerManager().ClearTimer(RollBackDrainTimerHandle);
	}

	LocalPredictionMessageQueue.Reset();
	OutboundPredictionMessageQueue.Reset();
	PendingVerificationMessageQueue.Reset();
	OutboundRollBackMessageQueue.Reset();
	RollBackMessageQueue.Reset();
	PredictionRecords.Reset();
	LastAppliedRollBackPredictionId = 0;
	bLocalPredictionDrainScheduled = false;
	bRollBackDrainScheduled = false;
	bHasInitialStateSnapshot = false;

	Super::EndPlay(EndPlayReason);
}

FCommonQTEPerformerPredictionMessage ACommonQTEPerformerActor::SubmitPrediction(int32 CellContent)
{
	if (!bHasInitialStateSnapshot)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("SubmitPrediction rejected because initial state is missing. Performer=%s CellContent=%d"), *GetNameSafe(this), CellContent);
		return FCommonQTEPerformerPredictionMessage();
	}

	const FCommonQTEPerformerPredictionMessage Message = BuildPredictionMessage(CellContent);
	EnqueuePredictionMessage(Message);
	FlushPredictionMessagesToServer();
	UE_LOG(LogCommonQTE, Log, TEXT("Prediction submitted. Handle=%d PredictionId=%d CellIndex=%d CellContent=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, Message.CellIndex, Message.CellContent, *GetNameSafe(this));
	return Message;
}

void ACommonQTEPerformerActor::SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot)
{
	InitialStateSnapshot = InInitialStateSnapshot;
	ResetPredictedStateFromInitialState();
	RouteInitialStatePresentationMessages();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Performer initial state snapshot set. Handle=%d Performer=%s CellCount=%d"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), *GetNameSafe(this), InitialStateSnapshot.CellContents.Num());
	ReceiveCommonQTEPresentationStateChanged();
	if (!RollBackMessageQueue.IsEmpty())
	{
		ScheduleRollBackDrain();
	}
}

const FCommonQTEStateSnapshot& ACommonQTEPerformerActor::GetInitialStateSnapshot() const
{
	return InitialStateSnapshot;
}

const FCommonQTEStateSnapshot& ACommonQTEPerformerActor::GetPredictedStateSnapshot() const
{
	return PredictedStateSnapshot;
}

FCommonQTEHandle ACommonQTEPerformerActor::GetCommonQTEHandle() const
{
	return bHasInitialStateSnapshot ? PredictedStateSnapshot.WholeHandle : InitialStateSnapshot.WholeHandle;
}

bool ACommonQTEPerformerActor::HasCommonQTEPresentationStateSnapshot() const
{
	return bHasInitialStateSnapshot || InitialStateSnapshot.WholeHandle.IsValid();
}

FCommonQTEStateSnapshot ACommonQTEPerformerActor::GetCommonQTEPresentationStateSnapshot() const
{
	return bHasInitialStateSnapshot ? PredictedStateSnapshot : InitialStateSnapshot;
}

void ACommonQTEPerformerActor::EnqueuePredictionMessage(const FCommonQTEPerformerPredictionMessage& Message)
{
	if (!bHasInitialStateSnapshot)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("EnqueuePredictionMessage rejected because initial state is missing. Performer=%s PredictionId=%d"), *GetNameSafe(this), Message.PredictionId);
		return;
	}

	const FCommonQTEPerformerPredictionMessage NormalizedMessage = NormalizePredictionMessage(Message);
	LocalPredictionMessageQueue.Add(NormalizedMessage);
	OutboundPredictionMessageQueue.Add(NormalizedMessage);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction message enqueued. Handle=%d PredictionId=%d CellIndex=%d CellContent=%d LocalQueue=%d OutboundQueue=%d"), FCommonQTELog::HandleValue(NormalizedMessage.WholeHandle), NormalizedMessage.PredictionId, NormalizedMessage.CellIndex, NormalizedMessage.CellContent, LocalPredictionMessageQueue.Num(), OutboundPredictionMessageQueue.Num());
	ScheduleLocalPredictionDrain();
}

void ACommonQTEPerformerActor::EnqueueRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message)
{
	const APlayerController* OwnerPlayerController = Cast<APlayerController>(GetOwner());
	if (HasAuthority() && OwnerPlayerController != nullptr && OwnerPlayerController->NetConnection != nullptr)
	{
		OutboundRollBackMessageQueue.Add(Message);
		UE_LOG(LogCommonQTE, Verbose, TEXT("Rollback message enqueued for remote client. Handle=%d PredictionId=%d QueueCount=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, OutboundRollBackMessageQueue.Num(), *GetNameSafe(this));
		return;
	}

	RollBackMessageQueue.Add(Message);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Rollback message enqueued locally. Handle=%d PredictionId=%d QueueCount=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, RollBackMessageQueue.Num(), *GetNameSafe(this));
	ScheduleRollBackDrain();
}

void ACommonQTEPerformerActor::FlushPredictionMessagesToServer()
{
	TArray<FCommonQTEPerformerPredictionMessage> Messages = OutboundPredictionMessageQueue;
	OutboundPredictionMessageQueue.Reset();
	if (Messages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("FlushPredictionMessagesToServer skipped because outbound queue is empty. Performer=%s"), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction messages looped back locally on authority. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
		ReceivePredictionMessages(Messages);
	}
	else
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction messages sent to server. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
		SendPredictionMessagesToServer(Messages);
	}
}

void ACommonQTEPerformerActor::FlushRollBackMessagesToClient()
{
	TArray<FCommonQTEPerformerRollBackMessage> Messages = OutboundRollBackMessageQueue;
	OutboundRollBackMessageQueue.Reset();
	if (Messages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("FlushRollBackMessagesToClient skipped because outbound queue is empty. Performer=%s"), *GetNameSafe(this));
		return;
	}

	const APlayerController* OwnerPlayerController = Cast<APlayerController>(GetOwner());
	if (HasAuthority() && OwnerPlayerController != nullptr && OwnerPlayerController->NetConnection != nullptr)
	{
		UE_LOG(LogCommonQTE, Log, TEXT("Rollback messages sent to client. Count=%d Performer=%s Owner=%s"), Messages.Num(), *GetNameSafe(this), *GetNameSafe(OwnerPlayerController));
		SendRollBackMessagesToClient(Messages);
	}
}

void ACommonQTEPerformerActor::AcknowledgePredictionMessage(int32 PredictionId)
{
	if (PredictionId <= 0)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("AcknowledgePredictionMessage rejected invalid prediction id. PredictionId=%d Performer=%s"), PredictionId, *GetNameSafe(this));
		return;
	}

	const APlayerController* OwnerPlayerController = Cast<APlayerController>(GetOwner());
	if (HasAuthority() && OwnerPlayerController != nullptr && OwnerPlayerController->NetConnection != nullptr)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction ack sent to client. PredictionId=%d Performer=%s Owner=%s"), PredictionId, *GetNameSafe(this), *GetNameSafe(OwnerPlayerController));
		SendPredictionAckToClient(PredictionId);
		return;
	}

	PrunePredictionRecordsUpTo(PredictionId);
}

TArray<FCommonQTEPerformerPredictionMessage> ACommonQTEPerformerActor::ConsumeLocalPredictionMessages()
{
	TArray<FCommonQTEPerformerPredictionMessage> Messages = LocalPredictionMessageQueue;
	LocalPredictionMessageQueue.Reset();
	return Messages;
}

TArray<FCommonQTEPerformerPredictionMessage> ACommonQTEPerformerActor::ConsumePendingVerificationMessages()
{
	TArray<FCommonQTEPerformerPredictionMessage> Messages = PendingVerificationMessageQueue;
	PendingVerificationMessageQueue.Reset();
	return Messages;
}

TArray<FCommonQTEPerformerRollBackMessage> ACommonQTEPerformerActor::ConsumeRollBackMessages()
{
	TArray<FCommonQTEPerformerRollBackMessage> Messages = RollBackMessageQueue;
	RollBackMessageQueue.Reset();
	return Messages;
}

void ACommonQTEPerformerActor::SendPredictionMessagesToServer_Implementation(const TArray<FCommonQTEPerformerPredictionMessage>& Messages)
{
	UE_LOG(LogCommonQTE, Verbose, TEXT("Server received prediction RPC. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
	ReceivePredictionMessages(Messages);
}

void ACommonQTEPerformerActor::SendRollBackMessagesToClient_Implementation(const TArray<FCommonQTEPerformerRollBackMessage>& Messages)
{
	UE_LOG(LogCommonQTE, Log, TEXT("Client received rollback RPC. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
	ReceiveRollBackMessages(Messages);
}

void ACommonQTEPerformerActor::SendPredictionAckToClient_Implementation(int32 PredictionId)
{
	UE_LOG(LogCommonQTE, Verbose, TEXT("Client received prediction ack RPC. PredictionId=%d Performer=%s"), PredictionId, *GetNameSafe(this));
	PrunePredictionRecordsUpTo(PredictionId);
}

void ACommonQTEPerformerActor::OnRep_InitialStateSnapshot()
{
	ResetPredictedStateFromInitialState();
	RouteInitialStatePresentationMessages();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Performer initial state snapshot replicated. Handle=%d Performer=%s CellCount=%d"), FCommonQTELog::HandleValue(InitialStateSnapshot.WholeHandle), *GetNameSafe(this), InitialStateSnapshot.CellContents.Num());
	ReceiveCommonQTEPresentationStateChanged();
	if (!RollBackMessageQueue.IsEmpty())
	{
		ScheduleRollBackDrain();
	}
}

void ACommonQTEPerformerActor::ReceivePredictionMessages(const TArray<FCommonQTEPerformerPredictionMessage>& Messages)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ReceivePredictionMessages rejected on non-authority. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
		return;
	}

	PendingVerificationMessageQueue.Append(Messages);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction messages received for verification. Count=%d PendingQueue=%d Performer=%s"), Messages.Num(), PendingVerificationMessageQueue.Num(), *GetNameSafe(this));

	UCommonQTEManagerComponent* Manager = FindManager();
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ReceivePredictionMessages failed because manager is missing. Performer=%s"), *GetNameSafe(this));
		return;
	}

	Manager->RequestVerificationDrain(this);
}

void ACommonQTEPerformerActor::ReceiveRollBackMessages(const TArray<FCommonQTEPerformerRollBackMessage>& Messages)
{
	RollBackMessageQueue.Append(Messages);
	UE_LOG(LogCommonQTE, Log, TEXT("Rollback messages received. Count=%d QueueCount=%d Performer=%s"), Messages.Num(), RollBackMessageQueue.Num(), *GetNameSafe(this));
	ScheduleRollBackDrain();
}

void ACommonQTEPerformerActor::ScheduleLocalPredictionDrain()
{
	if (bLocalPredictionDrainScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ACommonQTEPerformerActor::DrainLocalPredictionMessages);
	LocalPredictionDrainTimerHandle = World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	bLocalPredictionDrainScheduled = true;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Local prediction drain scheduled. QueueCount=%d Performer=%s"), LocalPredictionMessageQueue.Num(), *GetNameSafe(this));
}

void ACommonQTEPerformerActor::ScheduleRollBackDrain()
{
	if (bRollBackDrainScheduled)
	{
		return;
	}

	if (!bHasInitialStateSnapshot)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Rollback drain delayed because initial state snapshot is missing. QueueCount=%d Performer=%s"), RollBackMessageQueue.Num(), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ACommonQTEPerformerActor::DrainRollBackMessages);
	RollBackDrainTimerHandle = World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	bRollBackDrainScheduled = true;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Rollback drain scheduled. QueueCount=%d Performer=%s"), RollBackMessageQueue.Num(), *GetNameSafe(this));
}

void ACommonQTEPerformerActor::DrainLocalPredictionMessages()
{
	bLocalPredictionDrainScheduled = false;

	const TArray<FCommonQTEPerformerPredictionMessage> Messages = ConsumeLocalPredictionMessages();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Local prediction drain started. MessageCount=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
	TArray<FCommonQTEPerformerPredictionMessage> AppliedMessages;
	TArray<FCommonQTEPerformerPredictionRecord> AppliedRecords;
	for (const FCommonQTEPerformerPredictionMessage& Message : Messages)
	{
		FCommonQTEPerformerPredictionRecord PredictionRecord;
		if (ApplyPredictionMessage(Message, PredictionRecord))
		{
			PredictionRecords.Add(PredictionRecord);
			AppliedMessages.Add(PredictionRecord.PredictionMessage);
			AppliedRecords.Add(PredictionRecord);
		}
	}

	RoutePredictionPresentationMessages(AppliedRecords);

	if (!AppliedMessages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Log, TEXT("Local prediction messages applied. AppliedCount=%d Performer=%s"), AppliedMessages.Num(), *GetNameSafe(this));
		OnLocalPredictionMessagesReady.Broadcast(AppliedMessages);
	}
}

void ACommonQTEPerformerActor::DrainRollBackMessages()
{
	bRollBackDrainScheduled = false;

	if (!bHasInitialStateSnapshot)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Rollback drain skipped because initial state snapshot is missing. QueueCount=%d Performer=%s"), RollBackMessageQueue.Num(), *GetNameSafe(this));
		return;
	}

	const TArray<FCommonQTEPerformerRollBackMessage> Messages = ConsumeRollBackMessages();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Rollback drain started. MessageCount=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
	TArray<FCommonQTEPerformerRollBackMessage> AppliedMessages;
	for (const FCommonQTEPerformerRollBackMessage& Message : Messages)
	{
		FCommonQTEStateSnapshot StateBeforeRollBack;
		FCommonQTEStateSnapshot StateAfterRollBack;
		if (ApplyRollBackMessage(Message, StateBeforeRollBack, StateAfterRollBack))
		{
			AppliedMessages.Add(Message);
			RouteRollBackPresentationMessages(Message, StateBeforeRollBack, StateAfterRollBack);
		}
	}

	if (!AppliedMessages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Log, TEXT("Rollback messages applied. AppliedCount=%d Performer=%s"), AppliedMessages.Num(), *GetNameSafe(this));
		OnRollBackMessagesReady.Broadcast(AppliedMessages);
	}
}

void ACommonQTEPerformerActor::ResetPredictedStateFromInitialState()
{
	PredictedStateSnapshot = InitialStateSnapshot;
	PredictionRecords.Reset();
	LastAppliedRollBackPredictionId = 0;
	NextPredictionId = 1;
	bHasInitialStateSnapshot = InitialStateSnapshot.WholeHandle.IsValid();
	FCommonQTEStateRuntime::NormalizeStateSnapshot(PredictedStateSnapshot);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Predicted state reset from initial state. Handle=%d bHasInitialState=%s Performer=%s"), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), FCommonQTELog::BoolToString(bHasInitialStateSnapshot), *GetNameSafe(this));
}

void ACommonQTEPerformerActor::RouteInitialStatePresentationMessages()
{
	if (!bHasInitialStateSnapshot || !ShouldRoutePresentationMessages())
	{
		return;
	}

	UCommonQTEManagerComponent* Manager = FindManager();
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("RouteInitialStatePresentationMessages failed because manager is missing. Handle=%d Performer=%s"), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), *GetNameSafe(this));
		return;
	}

	TArray<FCommonQTEPresentationMessage> Messages;
	FCommonQTEPresentationBuilder::BuildInitialMessages(PredictedStateSnapshot, ECommonQTEPresentationSource::Performer, Messages);
	Manager->EnqueuePresentationMessages(Messages);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Initial presentation messages routed. Handle=%d Count=%d Performer=%s"), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), Messages.Num(), *GetNameSafe(this));
}

void ACommonQTEPerformerActor::RoutePredictionPresentationMessages(const TArray<FCommonQTEPerformerPredictionRecord>& PredictionRecordsToRoute)
{
	if (PredictionRecordsToRoute.IsEmpty() || !ShouldRoutePresentationMessages())
	{
		return;
	}

	UCommonQTEManagerComponent* Manager = FindManager();
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("RoutePredictionPresentationMessages failed because manager is missing. RecordCount=%d Performer=%s"), PredictionRecordsToRoute.Num(), *GetNameSafe(this));
		return;
	}

	TArray<FCommonQTEPresentationMessage> Messages;
	for (const FCommonQTEPerformerPredictionRecord& PredictionRecord : PredictionRecordsToRoute)
	{
		FCommonQTEPresentationBuilder::BuildSnapshotDiffMessages(
			PredictionRecord.StateBeforePrediction,
			PredictionRecord.StateAfterPrediction,
			ECommonQTEPresentationSource::Performer,
			ECommonQTEPresentationPhase::Prediction,
			PredictionRecord.PredictionId,
			Messages);
	}
	Manager->EnqueuePresentationMessages(Messages);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction presentation messages routed. RecordCount=%d MessageCount=%d Performer=%s"), PredictionRecordsToRoute.Num(), Messages.Num(), *GetNameSafe(this));
}

void ACommonQTEPerformerActor::RouteRollBackPresentationMessages(const FCommonQTEPerformerRollBackMessage& Message, const FCommonQTEStateSnapshot& StateBeforeRollBack, const FCommonQTEStateSnapshot& StateAfterRollBack)
{
	if (!ShouldRoutePresentationMessages())
	{
		return;
	}

	UCommonQTEManagerComponent* Manager = FindManager();
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("RouteRollBackPresentationMessages failed because manager is missing. Handle=%d PredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, *GetNameSafe(this));
		return;
	}

	TArray<FCommonQTEPresentationMessage> Messages;
	FCommonQTEPresentationBuilder::BuildSnapshotDiffMessages(StateBeforeRollBack, StateAfterRollBack, ECommonQTEPresentationSource::Performer, ECommonQTEPresentationPhase::Rollback, Message.PredictionId, Messages, Message.RejectedCellIndex);
	Manager->EnqueuePresentationMessages(Messages);
	UE_LOG(LogCommonQTE, Log, TEXT("Rollback presentation messages routed. Handle=%d PredictionId=%d MessageCount=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, Messages.Num(), *GetNameSafe(this));
}

FCommonQTEPerformerPredictionMessage ACommonQTEPerformerActor::BuildPredictionMessage(int32 CellContent)
{
	FCommonQTEPerformerPredictionMessage Message;
	Message.WholeHandle = PredictedStateSnapshot.WholeHandle;
	Message.PredictionId = NextPredictionId++;
	Message.CellIndex = PredictedStateSnapshot.DriverState.CellIndex;
	Message.CellContent = CellContent;
	return Message;
}

FCommonQTEPerformerPredictionMessage ACommonQTEPerformerActor::NormalizePredictionMessage(const FCommonQTEPerformerPredictionMessage& Message)
{
	FCommonQTEPerformerPredictionMessage NormalizedMessage = Message;
	if (!NormalizedMessage.WholeHandle.IsValid() && PredictedStateSnapshot.WholeHandle.IsValid())
	{
		NormalizedMessage.WholeHandle = PredictedStateSnapshot.WholeHandle;
	}
	if (NormalizedMessage.PredictionId <= 0)
	{
		NormalizedMessage.PredictionId = NextPredictionId++;
	}
	if (NormalizedMessage.CellIndex == INDEX_NONE)
	{
		NormalizedMessage.CellIndex = PredictedStateSnapshot.DriverState.CellIndex;
	}
	return NormalizedMessage;
}

bool ACommonQTEPerformerActor::ApplyPredictionMessage(const FCommonQTEPerformerPredictionMessage& Message, FCommonQTEPerformerPredictionRecord& OutRecord)
{
	if (!bHasInitialStateSnapshot || Message.WholeHandle != PredictedStateSnapshot.WholeHandle)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ApplyPredictionMessage rejected. bHasInitialState=%s MessageHandle=%d PredictedHandle=%d PredictionId=%d Performer=%s"), FCommonQTELog::BoolToString(bHasInitialStateSnapshot), FCommonQTELog::HandleValue(Message.WholeHandle), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), Message.PredictionId, *GetNameSafe(this));
		return false;
	}

	const FCommonQTEApplyInputResult ApplyResult = FCommonQTEStateRuntime::ApplyInput(PredictedStateSnapshot, Message.CellContent);
	if (!ApplyResult.bAccepted)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("ApplyPredictionMessage ignored unaccepted input. Handle=%d PredictionId=%d CellContent=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, Message.CellContent, *GetNameSafe(this));
		return false;
	}

	FCommonQTEPerformerPredictionMessage EffectiveMessage = Message;
	EffectiveMessage.CellIndex = ApplyResult.AppliedCellIndex;
	OutRecord.PredictionId = EffectiveMessage.PredictionId;
	OutRecord.PredictionMessage = EffectiveMessage;
	OutRecord.StateBeforePrediction = ApplyResult.StateBeforeInput;
	OutRecord.StateAfterPrediction = ApplyResult.StateAfterInput;
	PredictedStateSnapshot = ApplyResult.StateAfterInput;
	UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction applied locally. Handle=%d PredictionId=%d AppliedCellIndex=%d bMatched=%s State=%s Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, ApplyResult.AppliedCellIndex, FCommonQTELog::BoolToString(ApplyResult.bMatched), FCommonQTELog::StateToString(PredictedStateSnapshot.QTEState), *GetNameSafe(this));
	return true;
}

bool ACommonQTEPerformerActor::ApplyRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message, FCommonQTEStateSnapshot& OutStateBeforeRollBack, FCommonQTEStateSnapshot& OutStateAfterRollBack)
{
	if (Message.PredictionId > 0 && Message.PredictionId <= LastAppliedRollBackPredictionId)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("ApplyRollBackMessage ignored stale rollback. MessageHandle=%d PredictionId=%d LastAppliedRollBackPredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, LastAppliedRollBackPredictionId, *GetNameSafe(this));
		return false;
	}

	if (!Message.AuthoritativeStateSnapshot.WholeHandle.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ApplyRollBackMessage rejected because authoritative snapshot handle is invalid. MessageHandle=%d PredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, *GetNameSafe(this));
		return false;
	}

	if (PredictedStateSnapshot.WholeHandle.IsValid() && Message.AuthoritativeStateSnapshot.WholeHandle != PredictedStateSnapshot.WholeHandle)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ApplyRollBackMessage rejected because handle mismatched. MessageHandle=%d PredictedHandle=%d PredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.AuthoritativeStateSnapshot.WholeHandle), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), Message.PredictionId, *GetNameSafe(this));
		return false;
	}

	TArray<FCommonQTEPerformerPredictionRecord> PendingPredictionRecords;
	if (Message.PredictionId > 0)
	{
		for (const FCommonQTEPerformerPredictionRecord& PredictionRecord : PredictionRecords)
		{
			if (PredictionRecord.PredictionId > Message.PredictionId)
			{
				PendingPredictionRecords.Add(PredictionRecord);
			}
		}
		PendingPredictionRecords.Sort([](const FCommonQTEPerformerPredictionRecord& Left, const FCommonQTEPerformerPredictionRecord& Right)
		{
			return Left.PredictionId < Right.PredictionId;
		});
	}

	OutStateBeforeRollBack = PredictedStateSnapshot;
	const FCommonQTEStateSnapshot& RollBackBaseSnapshot = InitialStateSnapshot.WholeHandle.IsValid() ? InitialStateSnapshot : PredictedStateSnapshot;
	PredictedStateSnapshot = FCommonQTEStateRuntime::ExpandCompactStateSnapshot(Message.AuthoritativeStateSnapshot, RollBackBaseSnapshot);
	FCommonQTEStateRuntime::NormalizeStateSnapshot(PredictedStateSnapshot);

	PredictionRecords.Reset();
	LastAppliedRollBackPredictionId = Message.PredictionId > 0 ? FMath::Max(LastAppliedRollBackPredictionId, Message.PredictionId) : LastAppliedRollBackPredictionId;

	int32 ReplayedPredictionCount = 0;
	for (const FCommonQTEPerformerPredictionRecord& PendingPredictionRecord : PendingPredictionRecords)
	{
		FCommonQTEPerformerPredictionRecord ReplayedPredictionRecord;
		if (ApplyPredictionMessage(PendingPredictionRecord.PredictionMessage, ReplayedPredictionRecord))
		{
			PredictionRecords.Add(ReplayedPredictionRecord);
			++ReplayedPredictionCount;
		}
	}
	OutStateAfterRollBack = PredictedStateSnapshot;

	UE_LOG(LogCommonQTE, Log, TEXT("Rollback applied locally. Handle=%d PredictionId=%d RejectedCellIndex=%d PendingReplayCount=%d ReplayedPredictionCount=%d RemainingPredictionRecordCount=%d State=%s Performer=%s"), FCommonQTELog::HandleValue(PredictedStateSnapshot.WholeHandle), Message.PredictionId, Message.RejectedCellIndex, PendingPredictionRecords.Num(), ReplayedPredictionCount, PredictionRecords.Num(), FCommonQTELog::StateToString(PredictedStateSnapshot.QTEState), *GetNameSafe(this));
	return true;
}

void ACommonQTEPerformerActor::PrunePredictionRecordsUpTo(int32 PredictionId)
{
	if (PredictionId <= 0)
	{
		return;
	}

	const int32 OriginalRecordCount = PredictionRecords.Num();
	PredictionRecords.RemoveAll([PredictionId](const FCommonQTEPerformerPredictionRecord& PredictionRecord)
	{
		return PredictionRecord.PredictionId <= PredictionId;
	});

	const int32 RemovedRecordCount = OriginalRecordCount - PredictionRecords.Num();
	if (RemovedRecordCount > 0)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Prediction records pruned by ack. AckPredictionId=%d RemovedCount=%d RemainingCount=%d Performer=%s"), PredictionId, RemovedRecordCount, PredictionRecords.Num(), *GetNameSafe(this));
	}
	else
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Prediction ack had no matching records. AckPredictionId=%d RecordCount=%d Performer=%s"), PredictionId, PredictionRecords.Num(), *GetNameSafe(this));
	}
}

bool ACommonQTEPerformerActor::ShouldRoutePresentationMessages() const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Performer presentation routing skipped on dedicated server. Performer=%s"), *GetNameSafe(this));
		return false;
	}

	if (!HasAuthority())
	{
		return true;
	}

	const APlayerController* OwnerPlayerController = Cast<APlayerController>(GetOwner());
	const bool bShouldRoute = OwnerPlayerController == nullptr || OwnerPlayerController->NetConnection == nullptr;
	if (!bShouldRoute)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Performer presentation routing skipped on remote owner authority copy. Performer=%s Owner=%s"), *GetNameSafe(this), *GetNameSafe(OwnerPlayerController));
	}
	return bShouldRoute;
}

UCommonQTEManagerComponent* ACommonQTEPerformerActor::FindManager() const
{
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->FindComponentByClass<UCommonQTEManagerComponent>() : nullptr;
}
