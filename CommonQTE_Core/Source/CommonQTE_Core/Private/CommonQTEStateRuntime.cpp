#include "CommonQTEStateRuntime.h"

#include "CommonQTELog.h"

FCommonQTEApplyInputResult FCommonQTEStateRuntime::ApplyInput(const FCommonQTEStateSnapshot& CurrentState, int32 InputContent)
{
	FCommonQTEApplyInputResult Result;
	Result.StateBeforeInput = CurrentState;
	Result.StateAfterInput = CurrentState;
	NormalizeStateSnapshot(Result.StateBeforeInput);
	NormalizeStateSnapshot(Result.StateAfterInput);

	if (Result.StateAfterInput.QTEState != ECommonQTEState::OnGoing)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Runtime ignored input because QTE is not ongoing. Handle=%d InputContent=%d State=%s"), FCommonQTELog::HandleValue(Result.StateAfterInput.WholeHandle), InputContent, FCommonQTELog::StateToString(Result.StateAfterInput.QTEState));
		return Result;
	}

	const int32 CurrentCellIndex = Result.StateAfterInput.DriverState.CellIndex;
	Result.AppliedCellIndex = CurrentCellIndex;
	Result.bAccepted = true;

	if (!Result.StateAfterInput.CellContents.IsValidIndex(CurrentCellIndex))
	{
		Result.StateAfterInput.QTEState = ECommonQTEState::Fail;
		UE_LOG(LogCommonQTE, Log, TEXT("Runtime failed input because CellIndex is invalid. Handle=%d CellIndex=%d InputContent=%d CellCount=%d"), FCommonQTELog::HandleValue(Result.StateAfterInput.WholeHandle), CurrentCellIndex, InputContent, Result.StateAfterInput.CellContents.Num());
		return Result;
	}

	Result.bMatched = Result.StateAfterInput.CellContents[CurrentCellIndex] == InputContent;
	if (Result.StateAfterInput.CellStates.IsValidIndex(CurrentCellIndex))
	{
		Result.StateAfterInput.CellStates[CurrentCellIndex] = Result.bMatched ? ECommonQTECellState::Success : ECommonQTECellState::Fail;
	}

	ApplyCellResult(Result.StateAfterInput, Result.bMatched);
	UE_LOG(
		LogCommonQTE,
		Verbose,
		TEXT("Runtime applied input. Handle=%d CellIndex=%d ExpectedContent=%d InputContent=%d bMatched=%s ErrorCount=%d State=%s"),
		FCommonQTELog::HandleValue(Result.StateAfterInput.WholeHandle),
		CurrentCellIndex,
		Result.StateBeforeInput.CellContents.IsValidIndex(CurrentCellIndex) ? Result.StateBeforeInput.CellContents[CurrentCellIndex] : 0,
		InputContent,
		FCommonQTELog::BoolToString(Result.bMatched),
		Result.StateAfterInput.DriverState.ErrorCount,
		FCommonQTELog::StateToString(Result.StateAfterInput.QTEState));
	return Result;
}

void FCommonQTEStateRuntime::NormalizeStateSnapshot(FCommonQTEStateSnapshot& StateSnapshot)
{
	const int32 CellCount = StateSnapshot.CellContents.Num();
	if (StateSnapshot.CellStates.Num() < CellCount)
	{
		const int32 MissingCount = CellCount - StateSnapshot.CellStates.Num();
		for (int32 MissingIndex = 0; MissingIndex < MissingCount; ++MissingIndex)
		{
			StateSnapshot.CellStates.Add(ECommonQTECellState::OnGoing);
		}
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Runtime normalized state snapshot by adding cell states. Handle=%d MissingCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), MissingCount);
	}
	else if (StateSnapshot.CellStates.Num() > CellCount)
	{
		StateSnapshot.CellStates.SetNum(CellCount, EAllowShrinking::No);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Runtime normalized state snapshot by trimming cell states. Handle=%d CellCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), CellCount);
	}
}

FCommonQTECompactStateSnapshot FCommonQTEStateRuntime::BuildCompactStateSnapshot(const FCommonQTEStateSnapshot& StateSnapshot)
{
	FCommonQTEStateSnapshot NormalizedStateSnapshot = StateSnapshot;
	NormalizeStateSnapshot(NormalizedStateSnapshot);

	FCommonQTECompactStateSnapshot CompactStateSnapshot;
	CompactStateSnapshot.WholeHandle = NormalizedStateSnapshot.WholeHandle;
	CompactStateSnapshot.CellIndex = NormalizedStateSnapshot.DriverState.CellIndex;
	CompactStateSnapshot.ErrorCount = NormalizedStateSnapshot.DriverState.ErrorCount;
	CompactStateSnapshot.QTEState = NormalizedStateSnapshot.QTEState;
	CompactStateSnapshot.CellStates = NormalizedStateSnapshot.CellStates;
	return CompactStateSnapshot;
}

FCommonQTEStateSnapshot FCommonQTEStateRuntime::ExpandCompactStateSnapshot(const FCommonQTECompactStateSnapshot& CompactStateSnapshot, const FCommonQTEStateSnapshot& BaseStateSnapshot)
{
	FCommonQTEStateSnapshot ExpandedStateSnapshot = BaseStateSnapshot;
	if (CompactStateSnapshot.WholeHandle.IsValid())
	{
		ExpandedStateSnapshot.WholeHandle = CompactStateSnapshot.WholeHandle;
	}
	ExpandedStateSnapshot.DriverState.CellIndex = CompactStateSnapshot.CellIndex;
	ExpandedStateSnapshot.DriverState.ErrorCount = CompactStateSnapshot.ErrorCount;
	ExpandedStateSnapshot.QTEState = CompactStateSnapshot.QTEState;
	ExpandedStateSnapshot.CellStates = CompactStateSnapshot.CellStates;
	NormalizeStateSnapshot(ExpandedStateSnapshot);
	return ExpandedStateSnapshot;
}

bool FCommonQTEStateRuntime::IsTerminalState(ECommonQTEState State)
{
	return State == ECommonQTEState::Success || State == ECommonQTEState::Fail;
}

void FCommonQTEStateRuntime::ApplyCellResult(FCommonQTEStateSnapshot& StateSnapshot, bool bIsMatched)
{
	if (IsTerminalState(StateSnapshot.QTEState))
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Runtime skipped ApplyCellResult because state is terminal. Handle=%d State=%s"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), FCommonQTELog::StateToString(StateSnapshot.QTEState));
		return;
	}

	const int32 CellCount = StateSnapshot.CellContents.Num();
	if (CellCount <= 0)
	{
		StateSnapshot.DriverState.CellIndex = INDEX_NONE;
		StateSnapshot.QTEState = ECommonQTEState::Fail;
		UE_LOG(LogCommonQTE, Log, TEXT("Runtime failed QTE because CellCount is not positive. Handle=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle));
		return;
	}

	if (bIsMatched)
	{
		++StateSnapshot.DriverState.CellIndex;
		if (StateSnapshot.DriverState.CellIndex >= CellCount)
		{
			StateSnapshot.QTEState = ECommonQTEState::Success;
			UE_LOG(LogCommonQTE, Log, TEXT("Runtime completed QTE successfully. Handle=%d CellCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), CellCount);
		}
		return;
	}

	++StateSnapshot.DriverState.ErrorCount;
	switch (StateSnapshot.DriverState.Rule.IndexChangeTypeWhenErrorOccur)
	{
	case ECommonQTEIndexChangeTypeWhenErrorOccur::ResetToInitial:
		StateSnapshot.DriverState.CellIndex = 0;
		break;
	case ECommonQTEIndexChangeTypeWhenErrorOccur::MoveToNext:
		++StateSnapshot.DriverState.CellIndex;
		break;
	case ECommonQTEIndexChangeTypeWhenErrorOccur::Stay:
	default:
		break;
	}

	if (StateSnapshot.DriverState.ErrorCount > StateSnapshot.DriverState.Rule.MaxTolerableErrorCount)
	{
		StateSnapshot.QTEState = ECommonQTEState::Fail;
		UE_LOG(LogCommonQTE, Log, TEXT("Runtime failed QTE because max tolerable error count was exceeded. Handle=%d ErrorCount=%d MaxTolerableErrorCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), StateSnapshot.DriverState.ErrorCount, StateSnapshot.DriverState.Rule.MaxTolerableErrorCount);
		return;
	}

	if (StateSnapshot.DriverState.CellIndex >= CellCount)
	{
		StateSnapshot.QTEState = ECommonQTEState::Success;
		UE_LOG(LogCommonQTE, Log, TEXT("Runtime completed QTE after error index move. Handle=%d CellCount=%d"), FCommonQTELog::HandleValue(StateSnapshot.WholeHandle), CellCount);
	}
}
