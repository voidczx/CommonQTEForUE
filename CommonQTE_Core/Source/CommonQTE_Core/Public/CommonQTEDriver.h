#pragma once

#include "CoreMinimal.h"
#include "CommonQTECellGeneratePolicy.h"
#include "CommonQTEReplicationProxy.h"
#include "CommonQTEType.h"

class COMMONQTE_CORE_API FCommonQTEDriver
{
public:
	FCommonQTEDriver();
	~FCommonQTEDriver();

	void Initialize(const FCommonQTERule& InRule, const TSharedPtr<FCommonQTECellGeneratePolicy>& InCellGeneratePolicy);

	TArray<int32> GenerateCellContents() const;
	int32 GetCellIndex() const;
	int32 GetErrorCount() const;
	const FCommonQTERule& GetRule() const;
	void BuildStateSnapshot(FCommonQTEDriverStateSnapshot& OutSnapshot) const;
	void ApplyStateSnapshot(const FCommonQTEDriverStateSnapshot& Snapshot);
	FCommonQTEReplicationProxy& GetReplicationProxy();
	const FCommonQTEReplicationProxy& GetReplicationProxy() const;

private:
	FCommonQTERule Rule;
	TSharedPtr<FCommonQTECellGeneratePolicy> CellGeneratePolicy;
	int32 CellIndex = INDEX_NONE;
	int32 ErrorCount = 0;
	FCommonQTEReplicationProxy ReplicationProxy;
};
