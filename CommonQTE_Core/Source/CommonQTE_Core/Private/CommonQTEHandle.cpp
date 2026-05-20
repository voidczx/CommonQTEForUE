#include "CommonQTEHandle.h"

FCommonQTEHandle::FCommonQTEHandle() = default;

FCommonQTEHandle::FCommonQTEHandle(int32 InValue)
	: Value(InValue)
{
}

bool FCommonQTEHandle::IsValid() const
{
	return Value != InvalidValue;
}

int32 FCommonQTEHandle::GetValue() const
{
	return Value;
}

void FCommonQTEHandle::Reset()
{
	Value = InvalidValue;
}

bool FCommonQTEHandle::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Value;
	bOutSuccess = true;
	return true;
}

bool FCommonQTEHandle::operator==(const FCommonQTEHandle& Other) const
{
	return Value == Other.Value;
}

bool FCommonQTEHandle::operator!=(const FCommonQTEHandle& Other) const
{
	return !(*this == Other);
}

uint32 GetTypeHash(const FCommonQTEHandle& Handle)
{
	return GetTypeHash(Handle.Value);
}
