#include "CommonQTEDriver.h"

#include "CommonQTELog.h"

FCommonQTEDriver::FCommonQTEDriver() = default;

FCommonQTEDriver::~FCommonQTEDriver() = default;

void FCommonQTEDriver::Initialize(const FCommonQTERule& InRule, const TSharedPtr<FCommonQTECellGeneratePolicy>& InCellGeneratePolicy)
{
	Rule = InRule;
	CellGeneratePolicy = InCellGeneratePolicy;
	CellIndex = 0;
	ErrorCount = 0;
	UE_LOG(LogCommonQTE, Verbose, TEXT("Driver initialized. LimitTime=%.3f MaxTolerableErrorCount=%d ErrorIndexChange=%s bPolicyValid=%s"), Rule.LimitTime, Rule.MaxTolerableErrorCount, FCommonQTELog::IndexChangeTypeToString(Rule.IndexChangeTypeWhenErrorOccur), FCommonQTELog::BoolToString(CellGeneratePolicy.IsValid()));
}

TArray<int32> FCommonQTEDriver::GenerateCellContents() const
{
	if (!CellGeneratePolicy.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Driver failed to generate cells because CellGeneratePolicy is invalid."));
		return TArray<int32>();
	}

	TArray<int32> CellContents = CellGeneratePolicy->GenerateCellContents();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Driver generated cell contents. CellCount=%d"), CellContents.Num());
	return CellContents;
}

int32 FCommonQTEDriver::GetCellIndex() const
{
	return CellIndex;
}

int32 FCommonQTEDriver::GetErrorCount() const
{
	return ErrorCount;
}

const FCommonQTERule& FCommonQTEDriver::GetRule() const
{
	return Rule;
}

void FCommonQTEDriver::BuildStateSnapshot(FCommonQTEDriverStateSnapshot& OutSnapshot) const
{
	OutSnapshot.Rule = Rule;
	OutSnapshot.CellIndex = CellIndex;
	OutSnapshot.ErrorCount = ErrorCount;
}

void FCommonQTEDriver::ApplyStateSnapshot(const FCommonQTEDriverStateSnapshot& Snapshot)
{
	Rule = Snapshot.Rule;
	CellIndex = Snapshot.CellIndex;
	ErrorCount = Snapshot.ErrorCount;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Driver applied state snapshot. CellIndex=%d ErrorCount=%d"), CellIndex, ErrorCount);
}

FCommonQTEReplicationProxy& FCommonQTEDriver::GetReplicationProxy()
{
	return ReplicationProxy;
}

const FCommonQTEReplicationProxy& FCommonQTEDriver::GetReplicationProxy() const
{
	return ReplicationProxy;
}
