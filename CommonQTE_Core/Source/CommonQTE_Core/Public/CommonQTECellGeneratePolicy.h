#pragma once

#include "CoreMinimal.h"
#include "CommonQTEType.h"

class COMMONQTE_CORE_API FCommonQTECellGeneratePolicy
{
public:
	virtual ~FCommonQTECellGeneratePolicy();

	virtual TArray<int32> GenerateCellContents() const;
};

class COMMONQTE_CORE_API FCommonQTEFixedCellGeneratePolicy : public FCommonQTECellGeneratePolicy
{
public:
	explicit FCommonQTEFixedCellGeneratePolicy(const TArray<int32>& InCellContents);

	virtual TArray<int32> GenerateCellContents() const override;

private:
	TArray<int32> CellContents;
};

class COMMONQTE_CORE_API FCommonQTERandomCellGeneratePolicy : public FCommonQTECellGeneratePolicy
{
public:
	explicit FCommonQTERandomCellGeneratePolicy(const FCommonQTECellGenerateConfig& InGenerateConfig);

	virtual TArray<int32> GenerateCellContents() const override;

private:
	FCommonQTECellGenerateConfig GenerateConfig;
};
