#include "CommonQTEReplicationProxy.h"

#include "CommonQTELog.h"
#include "CommonQTEObserverActor.h"
#include "CommonQTEPerformerActor.h"

void FCommonQTEReplicationProxy::AddPerformer(ACommonQTEPerformerActor* Performer)
{
	if (Performer != nullptr)
	{
		Performers.AddUnique(Performer);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ReplicationProxy added performer. Performer=%s PerformerCount=%d"), *GetNameSafe(Performer), Performers.Num());
	}
}

void FCommonQTEReplicationProxy::RemovePerformer(ACommonQTEPerformerActor* Performer)
{
	Performers.Remove(Performer);
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ReplicationProxy removed performer. Performer=%s PerformerCount=%d"), *GetNameSafe(Performer), Performers.Num());
}

void FCommonQTEReplicationProxy::ResetPerformers()
{
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ReplicationProxy reset performers. PreviousCount=%d"), Performers.Num());
	Performers.Reset();
}

void FCommonQTEReplicationProxy::SetObserver(ACommonQTEObserverActor* InObserver)
{
	Observer = InObserver;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ReplicationProxy set observer. Observer=%s"), *GetNameSafe(InObserver));
}

ACommonQTEObserverActor* FCommonQTEReplicationProxy::GetObserver() const
{
	return Observer.Get();
}

void FCommonQTEReplicationProxy::ResetObserver()
{
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ReplicationProxy reset observer. PreviousObserver=%s"), *GetNameSafe(Observer.Get()));
	Observer.Reset();
}

const TArray<TWeakObjectPtr<ACommonQTEPerformerActor>>& FCommonQTEReplicationProxy::GetPerformers() const
{
	return Performers;
}

TArray<ACommonQTEPerformerActor*> FCommonQTEReplicationProxy::GetValidPerformers() const
{
	TArray<ACommonQTEPerformerActor*> ValidPerformers;
	for (const TWeakObjectPtr<ACommonQTEPerformerActor>& Performer : Performers)
	{
		if (ACommonQTEPerformerActor* PerformerActor = Performer.Get())
		{
			ValidPerformers.Add(PerformerActor);
		}
	}
	return ValidPerformers;
}
