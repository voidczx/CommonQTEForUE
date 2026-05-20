#include "CommonQTECellEntity.h"

#include "CommonQTELog.h"

FCommonQTECellEntity::FCommonQTECellEntity() = default;

FCommonQTECellEntity::FCommonQTECellEntity(const FCommonQTECellOwnerEntityInfo& InOwnerEntityInfo, int32 InCellContent)
	: OwnerEntityInfo(InOwnerEntityInfo)
	, CellContent(InCellContent)
{
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cell entity created. CellIndex=%d CellContent=%d"), OwnerEntityInfo.CellIndex, CellContent);
}

FCommonQTECellEntity::~FCommonQTECellEntity() = default;

const FCommonQTECellOwnerEntityInfo& FCommonQTECellEntity::GetOwnerEntityInfo() const
{
	return OwnerEntityInfo;
}

int32 FCommonQTECellEntity::GetCellContent() const
{
	return CellContent;
}

ECommonQTECellState FCommonQTECellEntity::GetState() const
{
	return State;
}

bool FCommonQTECellEntity::MatchInput(int32 InputContent) const
{
	return CellContent == InputContent;
}

void FCommonQTECellEntity::SetState(ECommonQTECellState InState)
{
	State = InState;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cell state set. CellIndex=%d CellContent=%d State=%s"), OwnerEntityInfo.CellIndex, CellContent, FCommonQTELog::CellStateToString(State));
}

void FCommonQTECellEntity::MarkSuccess()
{
	State = ECommonQTECellState::Success;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cell marked success. CellIndex=%d CellContent=%d"), OwnerEntityInfo.CellIndex, CellContent);
}

void FCommonQTECellEntity::MarkFail()
{
	State = ECommonQTECellState::Fail;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cell marked fail. CellIndex=%d CellContent=%d"), OwnerEntityInfo.CellIndex, CellContent);
}

void FCommonQTECellEntity::ResetState()
{
	State = ECommonQTECellState::OnGoing;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cell state reset. CellIndex=%d CellContent=%d"), OwnerEntityInfo.CellIndex, CellContent);
}
