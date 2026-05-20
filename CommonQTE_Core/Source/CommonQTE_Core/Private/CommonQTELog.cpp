#include "CommonQTELog.h"

DEFINE_LOG_CATEGORY(LogCommonQTE);

int32 FCommonQTELog::HandleValue(const FCommonQTEHandle& Handle)
{
	return Handle.GetValue();
}

const TCHAR* FCommonQTELog::BoolToString(bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

const TCHAR* FCommonQTELog::StateToString(ECommonQTEState State)
{
	switch (State)
	{
	case ECommonQTEState::OnGoing:
		return TEXT("OnGoing");
	case ECommonQTEState::Pause:
		return TEXT("Pause");
	case ECommonQTEState::Success:
		return TEXT("Success");
	case ECommonQTEState::Fail:
		return TEXT("Fail");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FCommonQTELog::CellStateToString(ECommonQTECellState State)
{
	switch (State)
	{
	case ECommonQTECellState::OnGoing:
		return TEXT("OnGoing");
	case ECommonQTECellState::Success:
		return TEXT("Success");
	case ECommonQTECellState::Fail:
		return TEXT("Fail");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FCommonQTELog::IndexChangeTypeToString(ECommonQTEIndexChangeTypeWhenErrorOccur IndexChangeType)
{
	switch (IndexChangeType)
	{
	case ECommonQTEIndexChangeTypeWhenErrorOccur::Stay:
		return TEXT("Stay");
	case ECommonQTEIndexChangeTypeWhenErrorOccur::ResetToInitial:
		return TEXT("ResetToInitial");
	case ECommonQTEIndexChangeTypeWhenErrorOccur::MoveToNext:
		return TEXT("MoveToNext");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FCommonQTELog::PresentationSourceToString(ECommonQTEPresentationSource Source)
{
	switch (Source)
	{
	case ECommonQTEPresentationSource::Observer:
		return TEXT("Observer");
	case ECommonQTEPresentationSource::Performer:
		return TEXT("Performer");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FCommonQTELog::PresentationPhaseToString(ECommonQTEPresentationPhase Phase)
{
	switch (Phase)
	{
	case ECommonQTEPresentationPhase::InitialState:
		return TEXT("InitialState");
	case ECommonQTEPresentationPhase::StateDelta:
		return TEXT("StateDelta");
	case ECommonQTEPresentationPhase::Prediction:
		return TEXT("Prediction");
	case ECommonQTEPresentationPhase::Rollback:
		return TEXT("Rollback");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* FCommonQTELog::NetModeToString(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone:
		return TEXT("Standalone");
	case NM_DedicatedServer:
		return TEXT("DedicatedServer");
	case NM_ListenServer:
		return TEXT("ListenServer");
	case NM_Client:
		return TEXT("Client");
	default:
		return TEXT("Unknown");
	}
}

FString FCommonQTELog::CellContentsToString(const TArray<int32>& CellContents, int32 MaxCount)
{
	const int32 ClampedMaxCount = FMath::Max(0, MaxCount);
	const int32 DisplayCount = FMath::Min(CellContents.Num(), ClampedMaxCount);

	FString Result(TEXT("["));
	for (int32 CellIndex = 0; CellIndex < DisplayCount; ++CellIndex)
	{
		if (CellIndex > 0)
		{
			Result += TEXT(",");
		}
		Result += FString::FromInt(CellContents[CellIndex]);
	}

	if (DisplayCount < CellContents.Num())
	{
		if (DisplayCount > 0)
		{
			Result += TEXT(",");
		}
		Result += TEXT("...");
	}

	Result += TEXT("]");
	return Result;
}
