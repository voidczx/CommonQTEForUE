#include "CommonQTECellGeneratePolicy.h"

#include "CommonQTELog.h"

FCommonQTECellGeneratePolicy::~FCommonQTECellGeneratePolicy() = default;

TArray<int32> FCommonQTECellGeneratePolicy::GenerateCellContents() const
{
	UE_LOG(LogCommonQTE, Warning, TEXT("Base cell generate policy was used. Returning empty CellContents."));
	return TArray<int32>();
}

FCommonQTEFixedCellGeneratePolicy::FCommonQTEFixedCellGeneratePolicy(const TArray<int32>& InCellContents)
	: CellContents(InCellContents)
{
}

TArray<int32> FCommonQTEFixedCellGeneratePolicy::GenerateCellContents() const
{
	if (CellContents.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Fixed cell generate policy produced empty CellContents."));
	}
	else
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Fixed cell generate policy produced cells. CellCount=%d"), CellContents.Num());
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Fixed cell contents: %s"), *FCommonQTELog::CellContentsToString(CellContents));
	}

	return CellContents;
}

FCommonQTERandomCellGeneratePolicy::FCommonQTERandomCellGeneratePolicy(const FCommonQTECellGenerateConfig& InGenerateConfig)
	: GenerateConfig(InGenerateConfig)
{
}

TArray<int32> FCommonQTERandomCellGeneratePolicy::GenerateCellContents() const
{
	UE_LOG(
		LogCommonQTE,
		Verbose,
		TEXT("Random cell generate policy started. InputTypeCount=%d CellCount=%d bUseRandomSeed=%s RandomSeed=%d"),
		GenerateConfig.InputTypeCount,
		GenerateConfig.CellCount,
		FCommonQTELog::BoolToString(GenerateConfig.bUseRandomSeed),
		GenerateConfig.RandomSeed);

	if (GenerateConfig.InputTypeCount <= 0 || GenerateConfig.CellCount <= 0)
	{
		UE_LOG(
			LogCommonQTE,
			Warning,
			TEXT("Random cell generate policy produced empty CellContents because config is invalid. InputTypeCount=%d CellCount=%d"),
			GenerateConfig.InputTypeCount,
			GenerateConfig.CellCount);
		return TArray<int32>();
	}

	FRandomStream RandomStream;
	if (GenerateConfig.bUseRandomSeed)
	{
		RandomStream.Initialize(GenerateConfig.RandomSeed);
	}
	else
	{
		RandomStream.GenerateNewSeed();
	}
	UE_LOG(LogCommonQTE, Verbose, TEXT("Random cell generate policy uses seed. Seed=%d"), RandomStream.GetInitialSeed());

	TArray<int32> CellContents;
	CellContents.Reserve(GenerateConfig.CellCount);
	for (int32 CellIndex = 0; CellIndex < GenerateConfig.CellCount; ++CellIndex)
	{
		CellContents.Add(RandomStream.RandRange(0, GenerateConfig.InputTypeCount - 1));
	}

	UE_LOG(
		LogCommonQTE,
		Verbose,
		TEXT("Random cell generate policy produced cells. GeneratedCount=%d InputTypeRange=[0,%d]"),
		CellContents.Num(),
		GenerateConfig.InputTypeCount - 1);
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Random cell contents: %s"), *FCommonQTELog::CellContentsToString(CellContents));
	return CellContents;
}
