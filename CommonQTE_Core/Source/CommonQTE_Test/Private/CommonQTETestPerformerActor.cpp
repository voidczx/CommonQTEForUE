#include "CommonQTETestPerformerActor.h"

#include "CommonQTETestLog.h"
#include "Engine/World.h"

void ACommonQTETestPerformerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearNetworkSimulationTimers();
	Super::EndPlay(EndPlayReason);
}

void ACommonQTETestPerformerActor::SetNetworkSimulationConfig(const FCommonQTETestNetworkSimulationConfig& InConfig)
{
	NetworkSimulationConfig = InConfig;
	NetworkSimulationConfig.PerformerToAuthorityDelaySeconds = FMath::Max(0.0f, NetworkSimulationConfig.PerformerToAuthorityDelaySeconds);
	NetworkSimulationConfig.AuthorityToPerformerDelaySeconds = FMath::Max(0.0f, NetworkSimulationConfig.AuthorityToPerformerDelaySeconds);
	NetworkSimulationConfig.AuthorityToObserverDelaySeconds = FMath::Max(0.0f, NetworkSimulationConfig.AuthorityToObserverDelaySeconds);
	NetworkSimulationConfig.VerificationSettleSeconds = FMath::Max(0.0f, NetworkSimulationConfig.VerificationSettleSeconds);
}

FCommonQTETestNetworkSimulationConfig ACommonQTETestPerformerActor::GetNetworkSimulationConfig() const
{
	return NetworkSimulationConfig;
}

int32 ACommonQTETestPerformerActor::GetPendingDelayedPredictionMessageCount() const
{
	return PendingDelayedPredictionMessageCount;
}

int32 ACommonQTETestPerformerActor::GetPendingDelayedRollBackMessageCount() const
{
	return PendingDelayedRollBackMessageCount;
}

FCommonQTEPerformerPredictionMessage ACommonQTETestPerformerActor::SubmitPredictionForTest(int32 LocalCellContent, int32 OutboundCellContent)
{
	if (!bHasInitialStateSnapshot)
	{
		UE_LOG(LogCommonQTETest, Warning, TEXT("SubmitPredictionForTest rejected because initial state is missing. Performer=%s LocalContent=%d OutboundContent=%d"), *GetNameSafe(this), LocalCellContent, OutboundCellContent);
		return FCommonQTEPerformerPredictionMessage();
	}

	const FCommonQTEPerformerPredictionMessage LocalMessage = NormalizePredictionMessage(BuildPredictionMessage(LocalCellContent));
	FCommonQTEPerformerPredictionMessage OutboundMessage = LocalMessage;
	OutboundMessage.CellContent = OutboundCellContent;
	OutboundMessage = NormalizePredictionMessage(OutboundMessage);

	LocalPredictionMessageQueue.Add(LocalMessage);
	OutboundPredictionMessageQueue.Add(OutboundMessage);
	ScheduleLocalPredictionDrain();
	FlushPredictionMessagesToServer();
	UE_LOG(LogCommonQTETest, Log, TEXT("Test prediction submitted. Handle=%d PredictionId=%d CellIndex=%d LocalContent=%d OutboundContent=%d Performer=%s"), LocalMessage.WholeHandle.GetValue(), LocalMessage.PredictionId, LocalMessage.CellIndex, LocalMessage.CellContent, OutboundMessage.CellContent, *GetNameSafe(this));
	return LocalMessage;
}

FCommonQTETestPerformerTrace ACommonQTETestPerformerActor::GetTestTrace() const
{
	return TestTrace;
}

void ACommonQTETestPerformerActor::ResetTestTrace()
{
	TestTrace = FCommonQTETestPerformerTrace();
}

void ACommonQTETestPerformerActor::SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot)
{
	Super::SetInitialStateSnapshot(InInitialStateSnapshot);
	TestTrace.bReceivedInitialState = true;
	TestTrace.LastInitialStateSnapshot = InInitialStateSnapshot;
}

void ACommonQTETestPerformerActor::EnqueueRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message)
{
	if (!ShouldDelayRollBackToPerformer())
	{
		Super::EnqueueRollBackMessage(Message);
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		Super::EnqueueRollBackMessage(Message);
		return;
	}

	++PendingDelayedRollBackMessageCount;
	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ACommonQTETestPerformerActor::DeliverDelayedRollBackMessage, Message);
	World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, NetworkSimulationConfig.AuthorityToPerformerDelaySeconds, false);
	NetworkSimulationTimerHandles.Add(TimerHandle);
	UE_LOG(LogCommonQTETest, Verbose, TEXT("Rollback message delayed for performer endpoint. Handle=%d PredictionId=%d Delay=%.3f Performer=%s"), Message.WholeHandle.GetValue(), Message.PredictionId, NetworkSimulationConfig.AuthorityToPerformerDelaySeconds, *GetNameSafe(this));
}

void ACommonQTETestPerformerActor::FlushPredictionMessagesToServer()
{
	if (!ShouldDelayPredictionToAuthority())
	{
		Super::FlushPredictionMessagesToServer();
		return;
	}

	TArray<FCommonQTEPerformerPredictionMessage> Messages = OutboundPredictionMessageQueue;
	OutboundPredictionMessageQueue.Reset();
	if (Messages.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		if (HasAuthority())
		{
			ReceivePredictionMessages(Messages);
		}
		else
		{
			SendPredictionMessagesToServer(Messages);
		}
		return;
	}

	PendingDelayedPredictionMessageCount += Messages.Num();
	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ACommonQTETestPerformerActor::DeliverDelayedPredictionMessages, Messages);
	World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, NetworkSimulationConfig.PerformerToAuthorityDelaySeconds, false);
	NetworkSimulationTimerHandles.Add(TimerHandle);
	UE_LOG(LogCommonQTETest, Verbose, TEXT("Prediction messages delayed for authority endpoint. Count=%d Delay=%.3f Performer=%s"), Messages.Num(), NetworkSimulationConfig.PerformerToAuthorityDelaySeconds, *GetNameSafe(this));
}

void ACommonQTETestPerformerActor::DrainLocalPredictionMessages()
{
	++TestTrace.LocalPredictionDrainCount;
	Super::DrainLocalPredictionMessages();
}

void ACommonQTETestPerformerActor::DrainRollBackMessages()
{
	++TestTrace.RollbackDrainCount;
	Super::DrainRollBackMessages();
}

void ACommonQTETestPerformerActor::RoutePredictionPresentationMessages(const TArray<FCommonQTEPerformerPredictionRecord>& PredictionRecordsToRoute)
{
	if (!PredictionRecordsToRoute.IsEmpty())
	{
		++TestTrace.PredictionPresentationRouteCount;
	}
	Super::RoutePredictionPresentationMessages(PredictionRecordsToRoute);
}

void ACommonQTETestPerformerActor::RouteRollBackPresentationMessages(const FCommonQTEPerformerRollBackMessage& Message, const FCommonQTEStateSnapshot& StateBeforeRollBack, const FCommonQTEStateSnapshot& StateAfterRollBack)
{
	++TestTrace.RollbackPresentationRouteCount;
	Super::RouteRollBackPresentationMessages(Message, StateBeforeRollBack, StateAfterRollBack);
}

bool ACommonQTETestPerformerActor::ApplyPredictionMessage(const FCommonQTEPerformerPredictionMessage& Message, FCommonQTEPerformerPredictionRecord& OutRecord)
{
	const bool bApplied = Super::ApplyPredictionMessage(Message, OutRecord);
	if (bApplied)
	{
		++TestTrace.PredictionApplyCount;
		TestTrace.LastStateBeforePrediction = OutRecord.StateBeforePrediction;
		TestTrace.LastStateAfterPrediction = OutRecord.StateAfterPrediction;
	}
	return bApplied;
}

bool ACommonQTETestPerformerActor::IsNetworkSimulationEnabled() const
{
	return NetworkSimulationConfig.bEnableNetworkSimulation && NetworkSimulationConfig.Topology != ECommonQTETestNetworkTopology::None;
}

bool ACommonQTETestPerformerActor::IsListenServerHostPerformer() const
{
	return IsNetworkSimulationEnabled()
		&& NetworkSimulationConfig.Topology == ECommonQTETestNetworkTopology::ListenServer
		&& NetworkSimulationConfig.ListenServerLayout == ECommonQTETestListenServerLayout::HostAsPerformer;
}

bool ACommonQTETestPerformerActor::ShouldDelayPredictionToAuthority() const
{
	return IsNetworkSimulationEnabled() && !IsListenServerHostPerformer() && NetworkSimulationConfig.PerformerToAuthorityDelaySeconds > 0.0f;
}

bool ACommonQTETestPerformerActor::ShouldDelayRollBackToPerformer() const
{
	return IsNetworkSimulationEnabled() && !IsListenServerHostPerformer() && NetworkSimulationConfig.AuthorityToPerformerDelaySeconds > 0.0f;
}

void ACommonQTETestPerformerActor::DeliverDelayedPredictionMessages(TArray<FCommonQTEPerformerPredictionMessage> Messages)
{
	PendingDelayedPredictionMessageCount = FMath::Max(0, PendingDelayedPredictionMessageCount - Messages.Num());
	if (Messages.IsEmpty())
	{
		return;
	}

	if (HasAuthority())
	{
		ReceivePredictionMessages(Messages);
	}
	else
	{
		SendPredictionMessagesToServer(Messages);
	}
	UE_LOG(LogCommonQTETest, Verbose, TEXT("Delayed prediction messages delivered to authority endpoint. Count=%d Performer=%s"), Messages.Num(), *GetNameSafe(this));
}

void ACommonQTETestPerformerActor::DeliverDelayedRollBackMessage(FCommonQTEPerformerRollBackMessage Message)
{
	PendingDelayedRollBackMessageCount = FMath::Max(0, PendingDelayedRollBackMessageCount - 1);
	Super::EnqueueRollBackMessage(Message);
	Super::FlushRollBackMessagesToClient();
	UE_LOG(LogCommonQTETest, Verbose, TEXT("Delayed rollback message delivered to performer endpoint. Handle=%d PredictionId=%d Performer=%s"), Message.WholeHandle.GetValue(), Message.PredictionId, *GetNameSafe(this));
}

void ACommonQTETestPerformerActor::ClearNetworkSimulationTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : NetworkSimulationTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}

	NetworkSimulationTimerHandles.Reset();
	PendingDelayedPredictionMessageCount = 0;
	PendingDelayedRollBackMessageCount = 0;
}

bool ACommonQTETestPerformerActor::ApplyRollBackMessage(const FCommonQTEPerformerRollBackMessage& Message, FCommonQTEStateSnapshot& OutStateBeforeRollBack, FCommonQTEStateSnapshot& OutStateAfterRollBack)
{
	const bool bApplied = Super::ApplyRollBackMessage(Message, OutStateBeforeRollBack, OutStateAfterRollBack);
	if (bApplied)
	{
		++TestTrace.RollbackApplyCount;
		TestTrace.LastStateBeforeRollback = OutStateBeforeRollBack;
		TestTrace.LastStateAfterRollback = OutStateAfterRollBack;
	}
	return bApplied;
}
