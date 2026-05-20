#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include "CommonQTESettings.generated.h"

UCLASS(Config = CommonQTE, DefaultConfig)
class COMMONQTE_CORE_API UCommonQTESettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "CommonQTE", meta = (MetaClass = "/Script/CommonQTE_Core.CommonQTEPerformerActor", AllowAbstract = "false"))
	FSoftClassPath DefaultPerformerActorClass;

	UPROPERTY(Config, EditAnywhere, Category = "CommonQTE", meta = (MetaClass = "/Script/CommonQTE_Core.CommonQTEObserverActor", AllowAbstract = "false"))
	FSoftClassPath DefaultObserverActorClass;

	UPROPERTY(Config, EditAnywhere, Category = "CommonQTE", meta = (ClampMin = "0"))
	int32 PresentationMessageQueueWarningThreshold = 100;
};
