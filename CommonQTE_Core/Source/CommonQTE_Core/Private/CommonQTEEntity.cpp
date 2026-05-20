#include "CommonQTEEntity.h"

#include "CommonQTECellEntity.h"
#include "CommonQTELog.h"
#include "CommonQTEStateRuntime.h"

FCommonQTEEntity::FCommonQTEEntity() = default;

FCommonQTEEntity::FCommonQTEEntity(const FCommonQTEHandle& InWholeHandle)
	: WholeHandle(InWholeHandle)
{
}

FCommonQTEEntity::~FCommonQTEEntity() = default;

void FCommonQTEEntity::Initialize(const FCommonQTERule& Rule, const TSharedPtr<FCommonQTECellGeneratePolicy>& CellGeneratePolicy, const TSharedPtr<FCommonQTEEntity>& Self)
{
	Driver.Initialize(Rule, CellGeneratePolicy);
	GenerateCells(Self);
	State = ECommonQTEState::OnGoing;
	UE_LOG(LogCommonQTE, Log, TEXT("Entity initialized. Handle=%d CellCount=%d LimitTime=%.3f MaxTolerableErrorCount=%d"), FCommonQTELog::HandleValue(WholeHandle), CellEntityArray.Num(), Rule.LimitTime, Rule.MaxTolerableErrorCount);
}

FCommonQTEHandle FCommonQTEEntity::GetHandle() const
{
	return WholeHandle;
}

ECommonQTEState FCommonQTEEntity::GetState() const
{
	return State;
}

ECommonQTECellState FCommonQTEEntity::GetCellState(int32 CellIndex) const
{
	return CellEntityArray.IsValidIndex(CellIndex) && CellEntityArray[CellIndex].IsValid()
		? CellEntityArray[CellIndex]->GetState()
		: ECommonQTECellState::Fail;
}

int32 FCommonQTEEntity::GetCellContent(int32 CellIndex) const
{
	return CellEntityArray.IsValidIndex(CellIndex) && CellEntityArray[CellIndex].IsValid()
		? CellEntityArray[CellIndex]->GetCellContent()
		: 0;
}

int32 FCommonQTEEntity::GetCellNum() const
{
	return CellEntityArray.Num();
}

void FCommonQTEEntity::Start()
{
	if (CellEntityArray.IsEmpty())
	{
		Success();
		UE_LOG(LogCommonQTE, Log, TEXT("Entity started with empty cells and completed immediately. Handle=%d"), FCommonQTELog::HandleValue(WholeHandle));
		return;
	}

	State = ECommonQTEState::OnGoing;
	UE_LOG(LogCommonQTE, Log, TEXT("Entity started. Handle=%d CellCount=%d"), FCommonQTELog::HandleValue(WholeHandle), CellEntityArray.Num());
}

void FCommonQTEEntity::Pause()
{
	if (State == ECommonQTEState::OnGoing)
	{
		State = ECommonQTEState::Pause;
		UE_LOG(LogCommonQTE, Log, TEXT("Entity paused. Handle=%d"), FCommonQTELog::HandleValue(WholeHandle));
	}
}

void FCommonQTEEntity::Resume()
{
	if (State == ECommonQTEState::Pause)
	{
		State = ECommonQTEState::OnGoing;
		UE_LOG(LogCommonQTE, Log, TEXT("Entity resumed. Handle=%d"), FCommonQTELog::HandleValue(WholeHandle));
	}
}

void FCommonQTEEntity::Success()
{
	State = ECommonQTEState::Success;
	UE_LOG(LogCommonQTE, Log, TEXT("Entity marked success. Handle=%d"), FCommonQTELog::HandleValue(WholeHandle));
}

void FCommonQTEEntity::Fail()
{
	State = ECommonQTEState::Fail;
	UE_LOG(LogCommonQTE, Log, TEXT("Entity marked fail. Handle=%d"), FCommonQTELog::HandleValue(WholeHandle));
}

bool FCommonQTEEntity::HandleInput(int32 InputContent)
{
	return HandleInputWithResult(InputContent).bMatched;
}

FCommonQTEApplyInputResult FCommonQTEEntity::HandleInputWithResult(int32 InputContent)
{
	FCommonQTEStateSnapshot CurrentSnapshot;
	BuildStateSnapshot(CurrentSnapshot);

	const FCommonQTEApplyInputResult Result = FCommonQTEStateRuntime::ApplyInput(CurrentSnapshot, InputContent);
	if (Result.bAccepted)
	{
		ApplyStateSnapshot(Result.StateAfterInput);
	}

	UE_LOG(LogCommonQTE, Verbose, TEXT("Entity handled input. Handle=%d InputContent=%d bAccepted=%s bMatched=%s AppliedCellIndex=%d State=%s"), FCommonQTELog::HandleValue(WholeHandle), InputContent, FCommonQTELog::BoolToString(Result.bAccepted), FCommonQTELog::BoolToString(Result.bMatched), Result.AppliedCellIndex, FCommonQTELog::StateToString(State));
	return Result;
}

TSharedPtr<FCommonQTECellEntity> FCommonQTEEntity::GetCurrentCell() const
{
	const int32 CellIndex = Driver.GetCellIndex();
	return CellEntityArray.IsValidIndex(CellIndex) ? CellEntityArray[CellIndex] : nullptr;
}

void FCommonQTEEntity::AddCell(const TSharedPtr<FCommonQTECellEntity>& CellEntity)
{
	if (CellEntity.IsValid())
	{
		CellEntityArray.Add(CellEntity);
	}
}

void FCommonQTEEntity::BuildStateSnapshot(FCommonQTEStateSnapshot& OutSnapshot) const
{
	OutSnapshot.WholeHandle = WholeHandle;
	Driver.BuildStateSnapshot(OutSnapshot.DriverState);
	OutSnapshot.QTEState = State;
	OutSnapshot.CellContents.Reset();
	OutSnapshot.CellStates.Reset();

	for (const TSharedPtr<FCommonQTECellEntity>& CellEntity : CellEntityArray)
	{
		if (!CellEntity.IsValid())
		{
			continue;
		}

		OutSnapshot.CellContents.Add(CellEntity->GetCellContent());
		OutSnapshot.CellStates.Add(CellEntity->GetState());
	}
}

void FCommonQTEEntity::ApplyStateSnapshot(const FCommonQTEStateSnapshot& Snapshot)
{
	if (Snapshot.WholeHandle.IsValid() && Snapshot.WholeHandle != WholeHandle)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Entity ignored state snapshot because handle mismatched. EntityHandle=%d SnapshotHandle=%d"), FCommonQTELog::HandleValue(WholeHandle), FCommonQTELog::HandleValue(Snapshot.WholeHandle));
		return;
	}

	State = Snapshot.QTEState;
	Driver.ApplyStateSnapshot(Snapshot.DriverState);

	for (int32 CellIndex = 0; CellIndex < CellEntityArray.Num(); ++CellIndex)
	{
		if (!CellEntityArray[CellIndex].IsValid())
		{
			continue;
		}

		const ECommonQTECellState CellState = Snapshot.CellStates.IsValidIndex(CellIndex)
			? Snapshot.CellStates[CellIndex]
			: ECommonQTECellState::OnGoing;
		CellEntityArray[CellIndex]->SetState(CellState);
	}
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Entity applied state snapshot. Handle=%d State=%s CellIndex=%d ErrorCount=%d"), FCommonQTELog::HandleValue(WholeHandle), FCommonQTELog::StateToString(State), Snapshot.DriverState.CellIndex, Snapshot.DriverState.ErrorCount);
}

void FCommonQTEEntity::BuildObserverStateDelta(int32 ChangedCellIndex, int32 SequenceId, FCommonQTEObserverStateDelta& OutDelta) const
{
	OutDelta.WholeHandle = WholeHandle;
	OutDelta.SequenceId = SequenceId;
	OutDelta.CellIndex = ChangedCellIndex;
	OutDelta.QTEState = State;

	if (CellEntityArray.IsValidIndex(ChangedCellIndex) && CellEntityArray[ChangedCellIndex].IsValid())
	{
		OutDelta.CellContent = CellEntityArray[ChangedCellIndex]->GetCellContent();
		OutDelta.CellState = CellEntityArray[ChangedCellIndex]->GetState();
	}
	else
	{
		OutDelta.CellContent = 0;
		OutDelta.CellState = ECommonQTECellState::OnGoing;
	}
}

FCommonQTEDriver& FCommonQTEEntity::GetDriver()
{
	return Driver;
}

const FCommonQTEDriver& FCommonQTEEntity::GetDriver() const
{
	return Driver;
}

void FCommonQTEEntity::GenerateCells(const TSharedPtr<FCommonQTEEntity>& Self)
{
	CellEntityArray.Reset();

	const TArray<int32> CellContents = Driver.GenerateCellContents();
	for (int32 CellIndex = 0; CellIndex < CellContents.Num(); ++CellIndex)
	{
		FCommonQTECellOwnerEntityInfo OwnerEntityInfo;
		OwnerEntityInfo.OwnerEntity = Self;
		OwnerEntityInfo.CellIndex = CellIndex;
		AddCell(MakeShared<FCommonQTECellEntity>(OwnerEntityInfo, CellContents[CellIndex]));
	}
	UE_LOG(LogCommonQTE, Verbose, TEXT("Entity generated cells. Handle=%d CellCount=%d Contents=%s"), FCommonQTELog::HandleValue(WholeHandle), CellEntityArray.Num(), *FCommonQTELog::CellContentsToString(CellContents));
}
