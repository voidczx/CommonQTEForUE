#include "CommonQTEPresentationBuilder.h"

#include "CommonQTELog.h"

void FCommonQTEPresentationBuilder::BuildInitialMessages(const FCommonQTEStateSnapshot& StateSnapshot, ECommonQTEPresentationSource Source, TArray<FCommonQTEPresentationMessage>& OutMessages)
{
	const int32 InitialCount = OutMessages.Num();
	for (int32 CellIndex = 0; CellIndex < StateSnapshot.CellContents.Num(); ++CellIndex)
	{
		FCommonQTEPresentationMessage Message;
		Message.WholeHandle = StateSnapshot.WholeHandle;
		Message.Source = Source;
		Message.Phase = ECommonQTEPresentationPhase::InitialState;
		Message.CellIndex = CellIndex;
		Message.CellContent = StateSnapshot.CellContents[CellIndex];
		Message.QTEState = StateSnapshot.QTEState;
		Message.CellState = StateSnapshot.CellStates.IsValidIndex(CellIndex) ? StateSnapshot.CellStates[CellIndex] : ECommonQTECellState::OnGoing;
		OutMessages.Add(Message);
	}
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Initial presentation messages built. Handle=%d Source=%s Phase=%s MessageCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), FCommonQTELog::PresentationSourceToString(Source), FCommonQTELog::PresentationPhaseToString(ECommonQTEPresentationPhase::InitialState), OutMessages.Num() - InitialCount);
}

void FCommonQTEPresentationBuilder::BuildStateDeltaMessage(const FCommonQTEObserverStateDelta& StateDelta, ECommonQTEPresentationSource Source, FCommonQTEPresentationMessage& OutMessage)
{
	OutMessage.WholeHandle = StateDelta.WholeHandle;
	OutMessage.Source = Source;
	OutMessage.Phase = ECommonQTEPresentationPhase::StateDelta;
	OutMessage.SequenceId = StateDelta.SequenceId;
	OutMessage.CellIndex = StateDelta.CellIndex;
	OutMessage.CellContent = StateDelta.CellContent;
	OutMessage.QTEState = StateDelta.QTEState;
	OutMessage.CellState = StateDelta.CellState;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("State delta presentation message built. Handle=%d Source=%s Phase=%s SequenceId=%d CellIndex=%d"), FCommonQTELog::HandleValue(StateDelta.WholeHandle), FCommonQTELog::PresentationSourceToString(Source), FCommonQTELog::PresentationPhaseToString(ECommonQTEPresentationPhase::StateDelta), StateDelta.SequenceId, StateDelta.CellIndex);
}

void FCommonQTEPresentationBuilder::BuildApplyInputResultMessages(const FCommonQTEApplyInputResult& ApplyResult, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, TArray<FCommonQTEPresentationMessage>& OutMessages)
{
	if (!ApplyResult.bAccepted)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Apply result presentation messages skipped because input was not accepted. Source=%s Phase=%s PredictionId=%d"), FCommonQTELog::PresentationSourceToString(Source), FCommonQTELog::PresentationPhaseToString(Phase), PredictionId);
		return;
	}

	BuildSnapshotDiffMessages(ApplyResult.StateBeforeInput, ApplyResult.StateAfterInput, Source, Phase, PredictionId, OutMessages);
}

void FCommonQTEPresentationBuilder::BuildSnapshotDiffMessages(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, TArray<FCommonQTEPresentationMessage>& OutMessages, int32 AppliedCellIndexOverride)
{
	const int32 CellContentCount = FMath::Max(BeforeSnapshot.CellContents.Num(), AfterSnapshot.CellContents.Num());
	const int32 CellStateCount = FMath::Max(BeforeSnapshot.CellStates.Num(), AfterSnapshot.CellStates.Num());
	const int32 CellCount = FMath::Max(CellContentCount, CellStateCount);
	const int32 InitialMessageCount = OutMessages.Num();
	const int32 AppliedCellIndex = AppliedCellIndexOverride != INDEX_NONE ? AppliedCellIndexOverride : BeforeSnapshot.DriverState.CellIndex;
	bool bHasAppliedCellMessage = false;
	bool bHasTerminalAppliedCellMessage = false;
	for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
	{
		if (IsCellChanged(BeforeSnapshot, AfterSnapshot, CellIndex))
		{
			FCommonQTEPresentationMessage Message = BuildSnapshotCellMessage(BeforeSnapshot, AfterSnapshot, Source, Phase, PredictionId, CellIndex, AppliedCellIndex);
			OutMessages.Add(Message);
			bHasAppliedCellMessage = bHasAppliedCellMessage || CellIndex == AppliedCellIndex;
			bHasTerminalAppliedCellMessage = bHasTerminalAppliedCellMessage || IsTerminalAppliedCellMessage(BeforeSnapshot, AfterSnapshot, CellIndex, AppliedCellIndex, Message.CellState);
		}
	}

	const bool bErrorCountIncreased = IsErrorCountIncreased(BeforeSnapshot, AfterSnapshot);
	if (bErrorCountIncreased && !bHasAppliedCellMessage)
	{
		OutMessages.Add(BuildSnapshotErrorInputMessage(BeforeSnapshot, AfterSnapshot, Source, Phase, PredictionId, AppliedCellIndex));
	}

	const bool bStateChanged = BeforeSnapshot.QTEState != AfterSnapshot.QTEState;
	const bool bTerminalStateChanged = bStateChanged && IsTerminalQTEState(AfterSnapshot.QTEState);
	const bool bTerminalInputStateChanged = bTerminalStateChanged && (bHasTerminalAppliedCellMessage || bErrorCountIncreased);
	if ((OutMessages.Num() == InitialMessageCount && bStateChanged) || bTerminalInputStateChanged)
	{
		OutMessages.Add(BuildSnapshotStateMessage(BeforeSnapshot, AfterSnapshot, Source, Phase, PredictionId));
	}
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Snapshot diff presentation messages built. Handle=%d Source=%s Phase=%s PredictionId=%d MessageCount=%d"), FCommonQTELog::HandleValue(AfterSnapshot.WholeHandle), FCommonQTELog::PresentationSourceToString(Source), FCommonQTELog::PresentationPhaseToString(Phase), PredictionId, OutMessages.Num() - InitialMessageCount);
}

bool FCommonQTEPresentationBuilder::IsCellChanged(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, int32 CellIndex)
{
	const bool bBeforeContentValid = BeforeSnapshot.CellContents.IsValidIndex(CellIndex);
	const bool bAfterContentValid = AfterSnapshot.CellContents.IsValidIndex(CellIndex);
	if (bBeforeContentValid != bAfterContentValid)
	{
		return true;
	}

	if (bBeforeContentValid && BeforeSnapshot.CellContents[CellIndex] != AfterSnapshot.CellContents[CellIndex])
	{
		return true;
	}

	const bool bBeforeStateValid = BeforeSnapshot.CellStates.IsValidIndex(CellIndex);
	const bool bAfterStateValid = AfterSnapshot.CellStates.IsValidIndex(CellIndex);
	if (bBeforeStateValid != bAfterStateValid)
	{
		return true;
	}

	return bBeforeStateValid && BeforeSnapshot.CellStates[CellIndex] != AfterSnapshot.CellStates[CellIndex];
}

bool FCommonQTEPresentationBuilder::IsErrorCountIncreased(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot)
{
	return AfterSnapshot.DriverState.ErrorCount > BeforeSnapshot.DriverState.ErrorCount;
}

bool FCommonQTEPresentationBuilder::IsTerminalQTEState(ECommonQTEState State)
{
	return State == ECommonQTEState::Success || State == ECommonQTEState::Fail;
}

bool FCommonQTEPresentationBuilder::IsTerminalAppliedCellMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, int32 CellIndex, int32 AppliedCellIndex, ECommonQTECellState CellState)
{
	return BeforeSnapshot.QTEState != AfterSnapshot.QTEState
		&& IsTerminalQTEState(AfterSnapshot.QTEState)
		&& CellIndex == AppliedCellIndex
		&& (CellState == ECommonQTECellState::Success || CellState == ECommonQTECellState::Fail);
}

FCommonQTEPresentationMessage FCommonQTEPresentationBuilder::BuildSnapshotCellMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, int32 CellIndex, int32 AppliedCellIndex)
{
	FCommonQTEPresentationMessage Message;
	Message.WholeHandle = AfterSnapshot.WholeHandle.IsValid() ? AfterSnapshot.WholeHandle : BeforeSnapshot.WholeHandle;
	Message.Source = Source;
	Message.Phase = Phase;
	Message.PredictionId = PredictionId;
	Message.CellIndex = CellIndex;
	Message.CellContent = AfterSnapshot.CellContents.IsValidIndex(CellIndex) ? AfterSnapshot.CellContents[CellIndex] : 0;
	Message.CellState = AfterSnapshot.CellStates.IsValidIndex(CellIndex) ? AfterSnapshot.CellStates[CellIndex] : ECommonQTECellState::OnGoing;
	Message.QTEState = IsTerminalAppliedCellMessage(BeforeSnapshot, AfterSnapshot, CellIndex, AppliedCellIndex, Message.CellState) ? BeforeSnapshot.QTEState : AfterSnapshot.QTEState;
	return Message;
}

FCommonQTEPresentationMessage FCommonQTEPresentationBuilder::BuildSnapshotErrorInputMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, int32 CellIndex)
{
	FCommonQTEPresentationMessage Message;
	Message.WholeHandle = AfterSnapshot.WholeHandle.IsValid() ? AfterSnapshot.WholeHandle : BeforeSnapshot.WholeHandle;
	Message.Source = Source;
	Message.Phase = Phase;
	Message.PredictionId = PredictionId;
	Message.CellIndex = CellIndex;
	Message.CellContent = AfterSnapshot.CellContents.IsValidIndex(CellIndex) ? AfterSnapshot.CellContents[CellIndex] : 0;
	Message.CellState = ECommonQTECellState::Fail;
	const bool bTerminalErrorStateChanged = BeforeSnapshot.QTEState != AfterSnapshot.QTEState && AfterSnapshot.QTEState == ECommonQTEState::Fail;
	Message.QTEState = bTerminalErrorStateChanged ? BeforeSnapshot.QTEState : AfterSnapshot.QTEState;
	return Message;
}

FCommonQTEPresentationMessage FCommonQTEPresentationBuilder::BuildSnapshotStateMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId)
{
	FCommonQTEPresentationMessage Message;
	Message.WholeHandle = AfterSnapshot.WholeHandle.IsValid() ? AfterSnapshot.WholeHandle : BeforeSnapshot.WholeHandle;
	Message.Source = Source;
	Message.Phase = Phase;
	Message.PredictionId = PredictionId;
	Message.CellIndex = INDEX_NONE;
	Message.QTEState = AfterSnapshot.QTEState;
	return Message;
}
