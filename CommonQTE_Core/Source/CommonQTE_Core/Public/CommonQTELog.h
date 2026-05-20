#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"
#include "Engine/EngineBaseTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCommonQTE, Warning, All);

struct COMMONQTE_CORE_API FCommonQTELog
{
	static int32 HandleValue(const FCommonQTEHandle& Handle);
	static const TCHAR* BoolToString(bool bValue);
	static const TCHAR* StateToString(ECommonQTEState State);
	static const TCHAR* CellStateToString(ECommonQTECellState State);
	static const TCHAR* IndexChangeTypeToString(ECommonQTEIndexChangeTypeWhenErrorOccur IndexChangeType);
	static const TCHAR* PresentationSourceToString(ECommonQTEPresentationSource Source);
	static const TCHAR* PresentationPhaseToString(ECommonQTEPresentationPhase Phase);
	static const TCHAR* NetModeToString(ENetMode NetMode);
	static FString CellContentsToString(const TArray<int32>& CellContents, int32 MaxCount = 16);
};
