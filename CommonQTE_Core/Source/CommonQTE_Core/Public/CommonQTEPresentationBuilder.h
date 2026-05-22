#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"

struct COMMONQTE_CORE_API FCommonQTEPresentationBuilder
{
	static void BuildInitialMessages(const FCommonQTEStateSnapshot& StateSnapshot, ECommonQTEPresentationSource Source, TArray<FCommonQTEPresentationMessage>& OutMessages);
	static void BuildStateDeltaMessage(const FCommonQTEObserverStateDelta& StateDelta, ECommonQTEPresentationSource Source, FCommonQTEPresentationMessage& OutMessage);
	static void BuildApplyInputResultMessages(const FCommonQTEApplyInputResult& ApplyResult, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, TArray<FCommonQTEPresentationMessage>& OutMessages);
	static void BuildSnapshotDiffMessages(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, TArray<FCommonQTEPresentationMessage>& OutMessages);

private:
	static bool IsCellChanged(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, int32 CellIndex);
	static bool IsErrorCountIncreased(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot);
	static bool IsTerminalQTEState(ECommonQTEState State);
	static bool IsTerminalAppliedCellMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, int32 CellIndex, ECommonQTECellState CellState);
	static FCommonQTEPresentationMessage BuildSnapshotCellMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, int32 CellIndex);
	static FCommonQTEPresentationMessage BuildSnapshotErrorInputMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId);
	static FCommonQTEPresentationMessage BuildSnapshotStateMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId);
};
