#pragma once

#include "CoreMinimal.h"
#include "CommonQTEDriver.h"
#include "CommonQTEHandle.h"
#include "CommonQTEType.h"

class FCommonQTECellEntity;

class COMMONQTE_CORE_API FCommonQTEEntity : public TSharedFromThis<FCommonQTEEntity>
{
public:
	FCommonQTEEntity();
	explicit FCommonQTEEntity(const FCommonQTEHandle& InWholeHandle);
	~FCommonQTEEntity();

	FCommonQTEEntity(const FCommonQTEEntity& Other) = delete;
	FCommonQTEEntity& operator=(const FCommonQTEEntity& Other) = delete;

	void Initialize(const FCommonQTERule& Rule, const TSharedPtr<FCommonQTECellGeneratePolicy>& CellGeneratePolicy, const TSharedPtr<FCommonQTEEntity>& Self);

	FCommonQTEHandle GetHandle() const;
	ECommonQTEState GetState() const;
	ECommonQTECellState GetCellState(int32 CellIndex) const;
	int32 GetCellContent(int32 CellIndex) const;
	int32 GetCellNum() const;

	void Start();
	void Pause();
	void Resume();
	void Success();
	void Fail();
	bool HandleInput(int32 InputContent);
	FCommonQTEApplyInputResult HandleInputWithResult(int32 InputContent);

	TSharedPtr<FCommonQTECellEntity> GetCurrentCell() const;
	void AddCell(const TSharedPtr<FCommonQTECellEntity>& CellEntity);
	void BuildStateSnapshot(FCommonQTEStateSnapshot& OutSnapshot) const;
	void ApplyStateSnapshot(const FCommonQTEStateSnapshot& Snapshot);
	void BuildObserverStateDelta(int32 ChangedCellIndex, int32 SequenceId, FCommonQTEObserverStateDelta& OutDelta) const;

	FCommonQTEDriver& GetDriver();
	const FCommonQTEDriver& GetDriver() const;

private:
	void GenerateCells(const TSharedPtr<FCommonQTEEntity>& Self);

	FCommonQTEHandle WholeHandle;
	FCommonQTEDriver Driver;
	TArray<TSharedPtr<FCommonQTECellEntity>> CellEntityArray;
	ECommonQTEState State = ECommonQTEState::OnGoing;
};
