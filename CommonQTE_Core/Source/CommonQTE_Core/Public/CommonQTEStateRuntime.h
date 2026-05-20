#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"

class COMMONQTE_CORE_API FCommonQTEStateRuntime
{
public:
	static FCommonQTEApplyInputResult ApplyInput(const FCommonQTEStateSnapshot& CurrentState, int32 InputContent);
	static void NormalizeStateSnapshot(FCommonQTEStateSnapshot& StateSnapshot);
	static FCommonQTECompactStateSnapshot BuildCompactStateSnapshot(const FCommonQTEStateSnapshot& StateSnapshot);
	static FCommonQTEStateSnapshot ExpandCompactStateSnapshot(const FCommonQTECompactStateSnapshot& CompactStateSnapshot, const FCommonQTEStateSnapshot& BaseStateSnapshot);

private:
	static bool IsTerminalState(ECommonQTEState State);
	static void ApplyCellResult(FCommonQTEStateSnapshot& StateSnapshot, bool bIsMatched);
};
