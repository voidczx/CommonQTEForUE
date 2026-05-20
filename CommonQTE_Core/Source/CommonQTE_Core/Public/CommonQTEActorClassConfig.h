#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "CommonQTEActorClassConfig.generated.h"

class ACommonQTEObserverActor;
class ACommonQTEPerformerActor;

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEActorClassConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE", meta = (AllowAbstract = "false"))
	TSubclassOf<ACommonQTEPerformerActor> PerformerActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE", meta = (AllowAbstract = "false"))
	TSubclassOf<ACommonQTEObserverActor> ObserverActorClass;
};
