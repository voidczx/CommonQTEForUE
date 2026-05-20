#pragma once

#include "CoreMinimal.h"

class ACommonQTEObserverActor;
class ACommonQTEPerformerActor;

class COMMONQTE_CORE_API FCommonQTEReplicationProxy
{
public:
	void AddPerformer(ACommonQTEPerformerActor* Performer);
	void RemovePerformer(ACommonQTEPerformerActor* Performer);
	void ResetPerformers();

	void SetObserver(ACommonQTEObserverActor* InObserver);
	ACommonQTEObserverActor* GetObserver() const;
	void ResetObserver();

	const TArray<TWeakObjectPtr<ACommonQTEPerformerActor>>& GetPerformers() const;
	TArray<ACommonQTEPerformerActor*> GetValidPerformers() const;

private:
	TArray<TWeakObjectPtr<ACommonQTEPerformerActor>> Performers;
	TWeakObjectPtr<ACommonQTEObserverActor> Observer;
};
