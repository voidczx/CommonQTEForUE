#pragma once

#include "CoreMinimal.h"

#include "CommonQTEHandle.generated.h"

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEHandle
{
	GENERATED_BODY()

public:
	static constexpr int32 InvalidValue = INDEX_NONE;

	FCommonQTEHandle();
	explicit FCommonQTEHandle(int32 InValue);

	bool IsValid() const;
	int32 GetValue() const;
	void Reset();

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	bool operator==(const FCommonQTEHandle& Other) const;
	bool operator!=(const FCommonQTEHandle& Other) const;

private:
	UPROPERTY()
	int32 Value = InvalidValue;

	friend COMMONQTE_CORE_API uint32 GetTypeHash(const FCommonQTEHandle& Handle);
};

COMMONQTE_CORE_API uint32 GetTypeHash(const FCommonQTEHandle& Handle);

template <>
struct TStructOpsTypeTraits<FCommonQTEHandle> : public TStructOpsTypeTraitsBase2<FCommonQTEHandle>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};
