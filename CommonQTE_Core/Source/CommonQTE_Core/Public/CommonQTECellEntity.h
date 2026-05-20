#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"

class COMMONQTE_CORE_API FCommonQTECellEntity
{
public:
	FCommonQTECellEntity();
	FCommonQTECellEntity(const FCommonQTECellOwnerEntityInfo& InOwnerEntityInfo, int32 InCellContent);
	~FCommonQTECellEntity();

	FCommonQTECellEntity(const FCommonQTECellEntity& Other) = delete;
	FCommonQTECellEntity& operator=(const FCommonQTECellEntity& Other) = delete;

	const FCommonQTECellOwnerEntityInfo& GetOwnerEntityInfo() const;
	int32 GetCellContent() const;
	ECommonQTECellState GetState() const;

	bool MatchInput(int32 InputContent) const;
	void SetState(ECommonQTECellState InState);
	void MarkSuccess();
	void MarkFail();
	void ResetState();

private:
	FCommonQTECellOwnerEntityInfo OwnerEntityInfo;
	int32 CellContent = 0;
	ECommonQTECellState State = ECommonQTECellState::OnGoing;
};
