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
	static FCommonQTEPresentationMessage BuildSnapshotCellMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId, int32 CellIndex);
	static FCommonQTEPresentationMessage BuildSnapshotStateMessage(const FCommonQTEStateSnapshot& BeforeSnapshot, const FCommonQTEStateSnapshot& AfterSnapshot, ECommonQTEPresentationSource Source, ECommonQTEPresentationPhase Phase, int32 PredictionId);
};
