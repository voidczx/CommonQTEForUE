#include "CommonQTEManagerComponent.h"

#include "CommonQTECellGeneratePolicy.h"
#include "CommonQTEEntity.h"
#include "CommonQTELog.h"
#include "CommonQTEObserverActor.h"
#include "CommonQTEPerformerActor.h"
#include "CommonQTESettings.h"
#include "CommonQTEStateRuntime.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UCommonQTEManagerComponent::UCommonQTEManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UCommonQTEManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Reset();

	Super::EndPlay(EndPlayReason);
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntity()
{
	return CreateEntity(FCommonQTERule(), FCommonQTECellGenerateConfig());
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntity(const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig)
{
	return CreateEntity(Rule, GenerateConfig, FCommonQTEActorClassConfig());
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntity(const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const FCommonQTEActorClassConfig& ActorClassConfig)
{
	return CreateEntityWithPolicy(Rule, MakeShared<FCommonQTERandomCellGeneratePolicy>(GenerateConfig), ActorClassConfig);
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntityWithFixedCells(const FCommonQTERule& Rule, const TArray<int32>& CellContents)
{
	return CreateEntityWithFixedCells(Rule, CellContents, FCommonQTEActorClassConfig());
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntityWithFixedCells(const FCommonQTERule& Rule, const TArray<int32>& CellContents, const FCommonQTEActorClassConfig& ActorClassConfig)
{
	return CreateEntityWithPolicy(Rule, MakeShared<FCommonQTEFixedCellGeneratePolicy>(CellContents), ActorClassConfig);
}

FCommonQTEHandle UCommonQTEManagerComponent::CreateEntityWithPolicy(const FCommonQTERule& Rule, const TSharedPtr<FCommonQTECellGeneratePolicy>& CellGeneratePolicy, const FCommonQTEActorClassConfig& ActorClassConfig)
{
	if (!IsAuthority() || !CellGeneratePolicy.IsValid())
	{
		UE_LOG(
			LogCommonQTE,
			Warning,
			TEXT("CreateEntityWithPolicy rejected. bAuthority=%s bPolicyValid=%s NetMode=%s"),
			FCommonQTELog::BoolToString(IsAuthority()),
			FCommonQTELog::BoolToString(CellGeneratePolicy.IsValid()),
			FCommonQTELog::NetModeToString(GetNetMode()));
		return FCommonQTEHandle();
	}

	UE_LOG(LogCommonQTE, Verbose, TEXT("CreateEntityWithPolicy generating cells. LimitTime=%.3f MaxTolerableErrorCount=%d ErrorIndexChange=%s"), Rule.LimitTime, Rule.MaxTolerableErrorCount, FCommonQTELog::IndexChangeTypeToString(Rule.IndexChangeTypeWhenErrorOccur));
	const TArray<int32> CellContents = CellGeneratePolicy->GenerateCellContents();
	UE_LOG(LogCommonQTE, Verbose, TEXT("CreateEntityWithPolicy generated cells. GeneratedCellCount=%d"), CellContents.Num());
	if (CellContents.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("CreateEntityWithPolicy failed because generated CellContents is empty."));
		return FCommonQTEHandle();
	}

	const FCommonQTEActorClassConfig ResolvedActorClassConfig = ResolveActorClassConfig(ActorClassConfig);
	const FCommonQTEHandle Handle(NextHandleValue++);
	const TSharedPtr<FCommonQTEEntity> Entity = MakeShared<FCommonQTEEntity>(Handle);
	Entity->Initialize(Rule, MakeShared<FCommonQTEFixedCellGeneratePolicy>(CellContents), Entity);
	Entity->Start();
	EntityMap.Add(Handle, Entity);
	EntityActorClassConfigMap.Add(Handle, ResolvedActorClassConfig);
	ObserverSequenceIdMap.Add(Handle, 0);

	if (Rule.LimitTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &UCommonQTEManagerComponent::HandleLimitTimeExpired, Handle);
			World->GetTimerManager().SetTimer(EntityTimers.FindOrAdd(Handle), TimerDelegate, Rule.LimitTime, false);
			UE_LOG(LogCommonQTE, Verbose, TEXT("Limit timer scheduled. Handle=%d LimitTime=%.3f"), FCommonQTELog::HandleValue(Handle), Rule.LimitTime);
		}
	}

	if (IsTerminalState(Entity->GetState()))
	{
		FinalizeEntity(Handle);
	}

	UE_LOG(LogCommonQTE, Log, TEXT("Entity created. Handle=%d CellCount=%d State=%s PerformerClass=%s ObserverClass=%s"), FCommonQTELog::HandleValue(Handle), CellContents.Num(), FCommonQTELog::StateToString(Entity->GetState()), *GetNameSafe(ResolvedActorClassConfig.PerformerActorClass.Get()), *GetNameSafe(ResolvedActorClassConfig.ObserverActorClass.Get()));
	return Handle;
}

bool UCommonQTEManagerComponent::RemoveEntity(const FCommonQTEHandle& Handle)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		ClearLimitTimer(Handle);
		ClearCleanupTimer(Handle);
		FinalizedEntities.Remove(Handle);
		EntityActorClassConfigMap.Remove(Handle);
		ObserverSequenceIdMap.Remove(Handle);
		UE_LOG(LogCommonQTE, Warning, TEXT("RemoveEntity failed because entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	const int32 PerformerCount = Entity->GetDriver().GetReplicationProxy().GetValidPerformers().Num();
	const bool bHasObserver = Entity->GetDriver().GetReplicationProxy().GetObserver() != nullptr;
	UE_LOG(LogCommonQTE, Log, TEXT("RemoveEntity started. Handle=%d PerformerCount=%d bHasObserver=%s"), FCommonQTELog::HandleValue(Handle), PerformerCount, FCommonQTELog::BoolToString(bHasObserver));

	ClearLimitTimer(Handle);
	ClearCleanupTimer(Handle);

	for (ACommonQTEPerformerActor* Performer : Entity->GetDriver().GetReplicationProxy().GetValidPerformers())
	{
		if (IsValid(Performer))
		{
			TWeakObjectPtr<ACommonQTEPerformerActor> WeakPerformer;
			WeakPerformer = Performer;
			DirtyVerificationPerformers.Remove(WeakPerformer);
			LastResolvedPredictionIdMap.Remove(TObjectKey<ACommonQTEPerformerActor>(Performer));
			Performer->Destroy();
		}
	}

	if (ACommonQTEObserverActor* Observer = Entity->GetDriver().GetReplicationProxy().GetObserver())
	{
		Observer->Destroy();
	}

	Entity->GetDriver().GetReplicationProxy().ResetPerformers();
	Entity->GetDriver().GetReplicationProxy().ResetObserver();
	FinalizedEntities.Remove(Handle);
	EntityActorClassConfigMap.Remove(Handle);
	ObserverSequenceIdMap.Remove(Handle);
	EntityMap.Remove(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("RemoveEntity completed. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	return true;
}

bool UCommonQTEManagerComponent::StopEntity(const FCommonQTEHandle& Handle)
{
	if (!IsAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StopEntity rejected on non-authority. Handle=%d NetMode=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::NetModeToString(GetNetMode()));
		return false;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StopEntity failed because entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	if (!IsTerminalState(Entity->GetState()))
	{
		Entity->Fail();
		UpdateObserverStateDelta(Handle, INDEX_NONE);
	}

	FinalizeEntity(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("StopEntity completed. Handle=%d State=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::StateToString(Entity->GetState()));
	return true;
}

bool UCommonQTEManagerComponent::PauseEntity(const FCommonQTEHandle& Handle)
{
	if (!IsAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("PauseEntity rejected on non-authority. Handle=%d NetMode=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::NetModeToString(GetNetMode()));
		return false;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("PauseEntity failed because entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	if (Entity->GetState() != ECommonQTEState::OnGoing)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("PauseEntity skipped because entity is not ongoing. Handle=%d State=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::StateToString(Entity->GetState()));
		return false;
	}

	Entity->Pause();
	if (FTimerHandle* TimerHandle = EntityTimers.Find(Handle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().PauseTimer(*TimerHandle);
		}
	}
	UpdateObserverStateDelta(Handle, INDEX_NONE);
	SyncPerformersToAuthoritativeState(Handle, 0, INDEX_NONE);
	UE_LOG(LogCommonQTE, Log, TEXT("PauseEntity completed. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	return true;
}

bool UCommonQTEManagerComponent::ResumeEntity(const FCommonQTEHandle& Handle)
{
	if (!IsAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ResumeEntity rejected on non-authority. Handle=%d NetMode=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::NetModeToString(GetNetMode()));
		return false;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ResumeEntity failed because entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	if (Entity->GetState() != ECommonQTEState::Pause)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("ResumeEntity skipped because entity is not paused. Handle=%d State=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::StateToString(Entity->GetState()));
		return false;
	}

	Entity->Resume();
	if (FTimerHandle* TimerHandle = EntityTimers.Find(Handle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().UnPauseTimer(*TimerHandle);
		}
	}
	UpdateObserverStateDelta(Handle, INDEX_NONE);
	SyncPerformersToAuthoritativeState(Handle, 0, INDEX_NONE);
	UE_LOG(LogCommonQTE, Log, TEXT("ResumeEntity completed. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	return true;
}

int32 UCommonQTEManagerComponent::PauseAllEntities()
{
	if (!IsAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("PauseAllEntities rejected on non-authority. NetMode=%s"), FCommonQTELog::NetModeToString(GetNetMode()));
		return 0;
	}

	TArray<FCommonQTEHandle> Handles;
	EntityMap.GetKeys(Handles);

	int32 PausedCount = 0;
	for (const FCommonQTEHandle& Handle : Handles)
	{
		if (PauseEntity(Handle))
		{
			++PausedCount;
		}
	}

	UE_LOG(LogCommonQTE, Log, TEXT("PauseAllEntities completed. RequestedCount=%d PausedCount=%d"), Handles.Num(), PausedCount);
	return PausedCount;
}

int32 UCommonQTEManagerComponent::ResumeAllEntities()
{
	if (!IsAuthority())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ResumeAllEntities rejected on non-authority. NetMode=%s"), FCommonQTELog::NetModeToString(GetNetMode()));
		return 0;
	}

	TArray<FCommonQTEHandle> Handles;
	EntityMap.GetKeys(Handles);

	int32 ResumedCount = 0;
	for (const FCommonQTEHandle& Handle : Handles)
	{
		if (ResumeEntity(Handle))
		{
			++ResumedCount;
		}
	}

	UE_LOG(LogCommonQTE, Log, TEXT("ResumeAllEntities completed. RequestedCount=%d ResumedCount=%d"), Handles.Num(), ResumedCount);
	return ResumedCount;
}

TSharedPtr<FCommonQTEEntity> UCommonQTEManagerComponent::FindEntity(const FCommonQTEHandle& Handle) const
{
	const TSharedPtr<FCommonQTEEntity>* Entity = EntityMap.Find(Handle);
	return Entity != nullptr ? *Entity : nullptr;
}

FCommonQTEActorClassConfig UCommonQTEManagerComponent::ResolveActorClassConfig(const FCommonQTEActorClassConfig& ActorClassConfig) const
{
	FCommonQTEActorClassConfig ResolvedConfig;
	ResolvedConfig.PerformerActorClass = ResolvePerformerActorClass(ActorClassConfig.PerformerActorClass);
	ResolvedConfig.ObserverActorClass = ResolveObserverActorClass(ActorClassConfig.ObserverActorClass);
	return ResolvedConfig;
}

TSubclassOf<ACommonQTEPerformerActor> UCommonQTEManagerComponent::ResolvePerformerActorClass(TSubclassOf<ACommonQTEPerformerActor> RuntimeActorClass) const
{
	if (RuntimeActorClass != nullptr)
	{
		UClass* RequestedActorClass = RuntimeActorClass.Get();
		if (RequestedActorClass != nullptr && !RequestedActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return RequestedActorClass;
		}

		UE_LOG(LogCommonQTE, Warning, TEXT("Runtime performer actor class is invalid. Class=%s FallbackClass=%s"), *GetNameSafe(RequestedActorClass), *GetNameSafe(ACommonQTEPerformerActor::StaticClass()));
		return ACommonQTEPerformerActor::StaticClass();
	}

	const UCommonQTESettings* Settings = GetDefault<UCommonQTESettings>();
	if (Settings != nullptr && !Settings->DefaultPerformerActorClass.IsNull())
	{
		UClass* ConfigActorClass = Settings->DefaultPerformerActorClass.TryLoadClass<ACommonQTEPerformerActor>();
		if (ConfigActorClass != nullptr && ConfigActorClass->IsChildOf(ACommonQTEPerformerActor::StaticClass()) && !ConfigActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return ConfigActorClass;
		}

		UE_LOG(LogCommonQTE, Warning, TEXT("Configured performer actor class is invalid. ClassPath=%s FallbackClass=%s"), *Settings->DefaultPerformerActorClass.ToString(), *GetNameSafe(ACommonQTEPerformerActor::StaticClass()));
	}

	return ACommonQTEPerformerActor::StaticClass();
}

TSubclassOf<ACommonQTEObserverActor> UCommonQTEManagerComponent::ResolveObserverActorClass(TSubclassOf<ACommonQTEObserverActor> RuntimeActorClass) const
{
	if (RuntimeActorClass != nullptr)
	{
		UClass* RequestedActorClass = RuntimeActorClass.Get();
		if (RequestedActorClass != nullptr && !RequestedActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return RequestedActorClass;
		}

		UE_LOG(LogCommonQTE, Warning, TEXT("Runtime observer actor class is invalid. Class=%s FallbackClass=%s"), *GetNameSafe(RequestedActorClass), *GetNameSafe(ACommonQTEObserverActor::StaticClass()));
		return ACommonQTEObserverActor::StaticClass();
	}

	const UCommonQTESettings* Settings = GetDefault<UCommonQTESettings>();
	if (Settings != nullptr && !Settings->DefaultObserverActorClass.IsNull())
	{
		UClass* ConfigActorClass = Settings->DefaultObserverActorClass.TryLoadClass<ACommonQTEObserverActor>();
		if (ConfigActorClass != nullptr && ConfigActorClass->IsChildOf(ACommonQTEObserverActor::StaticClass()) && !ConfigActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return ConfigActorClass;
		}

		UE_LOG(LogCommonQTE, Warning, TEXT("Configured observer actor class is invalid. ClassPath=%s FallbackClass=%s"), *Settings->DefaultObserverActorClass.ToString(), *GetNameSafe(ACommonQTEObserverActor::StaticClass()));
	}

	return ACommonQTEObserverActor::StaticClass();
}

FCommonQTEActorClassConfig UCommonQTEManagerComponent::GetEntityActorClassConfig(const FCommonQTEHandle& Handle) const
{
	const FCommonQTEActorClassConfig* ActorClassConfig = EntityActorClassConfigMap.Find(Handle);
	if (ActorClassConfig != nullptr)
	{
		return *ActorClassConfig;
	}

	UE_LOG(LogCommonQTE, Warning, TEXT("Entity actor class config is missing. Handle=%d Using native actor classes."), FCommonQTELog::HandleValue(Handle));
	FCommonQTEActorClassConfig NativeActorClassConfig;
	NativeActorClassConfig.PerformerActorClass = ACommonQTEPerformerActor::StaticClass();
	NativeActorClassConfig.ObserverActorClass = ACommonQTEObserverActor::StaticClass();
	return NativeActorClassConfig;
}

void UCommonQTEManagerComponent::Reset()
{
	TArray<FCommonQTEHandle> Handles;
	EntityMap.GetKeys(Handles);
	UE_LOG(LogCommonQTE, Log, TEXT("Manager reset started. EntityCount=%d"), Handles.Num());

	for (const FCommonQTEHandle& Handle : Handles)
	{
		RemoveEntity(Handle);
	}
	EntityMap.Reset();
	EntityTimers.Reset();
	EntityCleanupTimers.Reset();
	EntityActorClassConfigMap.Reset();
	FinalizedEntities.Reset();
	ObserverSequenceIdMap.Reset();
	DirtyVerificationPerformers.Reset();
	LastResolvedPredictionIdMap.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VerificationDrainTimerHandle);
		World->GetTimerManager().ClearTimer(PresentationDrainTimerHandle);
	}
	PresentationMessageQueue.Reset();
	PresentationMessageQueueHead = 0;
	bVerificationDrainScheduled = false;
	bPresentationDrainScheduled = false;
	bPresentationQueueThresholdReported = false;
	UE_LOG(LogCommonQTE, Log, TEXT("Manager reset completed."));
}

ACommonQTEPerformerActor* UCommonQTEManagerComponent::AddPerformer(const FCommonQTEHandle& Handle, AActor* PerformerOwner)
{
	if (!IsAuthority() || FinalizedEntities.Contains(Handle))
	{
		UE_LOG(
			LogCommonQTE,
			Warning,
			TEXT("AddPerformer rejected. Handle=%d bAuthority=%s bFinalized=%s PerformerOwner=%s"),
			FCommonQTELog::HandleValue(Handle),
			FCommonQTELog::BoolToString(IsAuthority()),
			FCommonQTELog::BoolToString(FinalizedEntities.Contains(Handle)),
			*GetNameSafe(PerformerOwner));
		return nullptr;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	UWorld* World = GetWorld();
	if (!Entity.IsValid() || World == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("AddPerformer failed. Handle=%d bEntityValid=%s bWorldValid=%s PerformerOwner=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(Entity.IsValid()), FCommonQTELog::BoolToString(World != nullptr), *GetNameSafe(PerformerOwner));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	APlayerController* PlayerController = ResolvePlayerController(PerformerOwner);
	if (PlayerController != nullptr)
	{
		SpawnParameters.Owner = PlayerController;
	}

	const FCommonQTEActorClassConfig ActorClassConfig = GetEntityActorClassConfig(Handle);
	ACommonQTEPerformerActor* Performer = World->SpawnActor<ACommonQTEPerformerActor>(ActorClassConfig.PerformerActorClass.Get(), FTransform::Identity, SpawnParameters);
	if (Performer == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("AddPerformer failed to spawn performer actor. Handle=%d Owner=%s PerformerClass=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(PlayerController), *GetNameSafe(ActorClassConfig.PerformerActorClass.Get()));
		return nullptr;
	}

	if (PlayerController != nullptr)
	{
		Performer->SetOwner(PlayerController);
	}

	if (PlayerController != nullptr && PlayerController->NetConnection != nullptr)
	{
		Performer->bOnlyRelevantToOwner = true;
		Performer->SetReplicates(true);
	}
	else
	{
		Performer->SetReplicates(false);
	}

	Entity->GetDriver().GetReplicationProxy().AddPerformer(Performer);
	LastResolvedPredictionIdMap.Add(TObjectKey<ACommonQTEPerformerActor>(Performer), 0);
	FCommonQTEStateSnapshot StateSnapshot;
	Entity->BuildStateSnapshot(StateSnapshot);
	Performer->SetInitialStateSnapshot(StateSnapshot);
	UpdateObserverExcludedPlayerControllers(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("Performer added. Handle=%d Performer=%s Owner=%s bReplicated=%s PerformerClass=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(Performer), *GetNameSafe(Performer->GetOwner()), FCommonQTELog::BoolToString(Performer->GetIsReplicated()), *GetNameSafe(ActorClassConfig.PerformerActorClass.Get()));
	return Performer;
}

ACommonQTEObserverActor* UCommonQTEManagerComponent::CreateObserver(const FCommonQTEHandle& Handle)
{
	if (!IsAuthority() || FinalizedEntities.Contains(Handle))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("CreateObserver rejected. Handle=%d bAuthority=%s bFinalized=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(IsAuthority()), FCommonQTELog::BoolToString(FinalizedEntities.Contains(Handle)));
		return nullptr;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	UWorld* World = GetWorld();
	if (!Entity.IsValid() || World == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("CreateObserver failed. Handle=%d bEntityValid=%s bWorldValid=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(Entity.IsValid()), FCommonQTELog::BoolToString(World != nullptr));
		return nullptr;
	}

	if (ACommonQTEObserverActor* ExistingObserver = Entity->GetDriver().GetReplicationProxy().GetObserver())
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("CreateObserver returned existing observer. Handle=%d Observer=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(ExistingObserver));
		return ExistingObserver;
	}

	const FCommonQTEActorClassConfig ActorClassConfig = GetEntityActorClassConfig(Handle);
	ACommonQTEObserverActor* Observer = World->SpawnActor<ACommonQTEObserverActor>(ActorClassConfig.ObserverActorClass.Get(), FTransform::Identity);
	if (Observer == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("CreateObserver failed to spawn observer actor. Handle=%d ObserverClass=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(ActorClassConfig.ObserverActorClass.Get()));
		return nullptr;
	}

	Entity->GetDriver().GetReplicationProxy().SetObserver(Observer);
	UpdateObserverExcludedPlayerControllers(Handle);
	UpdateInitialStateSnapshot(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("Observer created. Handle=%d Observer=%s ObserverClass=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(Observer), *GetNameSafe(ActorClassConfig.ObserverActorClass.Get()));
	return Observer;
}

TArray<ACommonQTEPerformerActor*> UCommonQTEManagerComponent::GetPerformers(const FCommonQTEHandle& Handle) const
{
	const TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	return Entity.IsValid() ? Entity->GetDriver().GetReplicationProxy().GetValidPerformers() : TArray<ACommonQTEPerformerActor*>();
}

ACommonQTEObserverActor* UCommonQTEManagerComponent::GetObserver(const FCommonQTEHandle& Handle) const
{
	const TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	return Entity.IsValid() ? Entity->GetDriver().GetReplicationProxy().GetObserver() : nullptr;
}

ECommonQTEState UCommonQTEManagerComponent::GetEntityState(const FCommonQTEHandle& Handle) const
{
	const TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	return Entity.IsValid() ? Entity->GetState() : ECommonQTEState::Fail;
}

ECommonQTECellState UCommonQTEManagerComponent::GetCellState(const FCommonQTEHandle& Handle, int32 CellIndex) const
{
	const TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	return Entity.IsValid() ? Entity->GetCellState(CellIndex) : ECommonQTECellState::Fail;
}

bool UCommonQTEManagerComponent::GetEntityStateSnapshot(const FCommonQTEHandle& Handle, FCommonQTEStateSnapshot& OutSnapshot) const
{
	const TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		OutSnapshot = FCommonQTEStateSnapshot();
		return false;
	}

	Entity->BuildStateSnapshot(OutSnapshot);
	return true;
}

void UCommonQTEManagerComponent::RequestVerificationDrain(ACommonQTEPerformerActor* Performer)
{
	if (!IsAuthority() || !IsValid(Performer))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("RequestVerificationDrain rejected. bAuthority=%s Performer=%s"), FCommonQTELog::BoolToString(IsAuthority()), *GetNameSafe(Performer));
		return;
	}

	TWeakObjectPtr<ACommonQTEPerformerActor> WeakPerformer;
	WeakPerformer = Performer;
	DirtyVerificationPerformers.AddUnique(WeakPerformer);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Verification drain requested. Performer=%s DirtyPerformerCount=%d"), *GetNameSafe(Performer), DirtyVerificationPerformers.Num());
	ScheduleVerificationDrain();
}

void UCommonQTEManagerComponent::EnqueuePresentationMessage(const FCommonQTEPresentationMessage& Message)
{
	PresentationMessageQueue.Add(Message);
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Presentation message enqueued. Handle=%d Source=%s Phase=%s CellIndex=%d QueueCount=%d"), FCommonQTELog::HandleValue(Message.WholeHandle), FCommonQTELog::PresentationSourceToString(Message.Source), FCommonQTELog::PresentationPhaseToString(Message.Phase), Message.CellIndex, GetPresentationMessageQueueCount());
	CheckPresentationMessageQueueThreshold(TEXT("EnqueuePresentationMessage"));
	if (PresentationDrainConfig.bAutoDrain)
	{
		SchedulePresentationDrain();
	}
}

void UCommonQTEManagerComponent::EnqueuePresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages)
{
	if (Messages.IsEmpty())
	{
		return;
	}

	PresentationMessageQueue.Append(Messages);
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Presentation messages enqueued. Count=%d QueueCount=%d"), Messages.Num(), GetPresentationMessageQueueCount());
	CheckPresentationMessageQueueThreshold(TEXT("EnqueuePresentationMessages"));
	if (PresentationDrainConfig.bAutoDrain)
	{
		SchedulePresentationDrain();
	}
}

TArray<FCommonQTEPresentationMessage> UCommonQTEManagerComponent::ConsumePresentationMessages()
{
	TArray<FCommonQTEPresentationMessage> Messages;
	while (!IsPresentationMessageQueueEmpty())
	{
		Messages.Append(ConsumePresentationMessages(GetPresentationMessageQueueCount()));
	}
	return Messages;
}

TArray<FCommonQTEPresentationMessage> UCommonQTEManagerComponent::ConsumePresentationMessages(int32 MaxMessages)
{
	TArray<FCommonQTEPresentationMessage> Messages;
	if (IsPresentationMessageQueueEmpty())
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ConsumePresentationMessages returned empty. MaxMessages=%d QueueCount=%d"), MaxMessages, GetPresentationMessageQueueCount());
		return Messages;
	}

	const FCommonQTEPresentationMessage* HeadMessage = PeekPresentationMessage();
	if (HeadMessage != nullptr && IsInitialPresentationMessage(*HeadMessage))
	{
		return ConsumeInitialPresentationBatch();
	}

	if (MaxMessages <= 0)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("ConsumePresentationMessages returned empty. MaxMessages=%d QueueCount=%d"), MaxMessages, GetPresentationMessageQueueCount());
		return Messages;
	}

	const int32 ConsumeCount = FMath::Min(MaxMessages, GetPresentationMessageQueueCount());
	Messages.Reserve(ConsumeCount);
	for (int32 MessageIndex = 0; MessageIndex < ConsumeCount; ++MessageIndex)
	{
		Messages.Add(PresentationMessageQueue[PresentationMessageQueueHead + MessageIndex]);
	}
	PresentationMessageQueueHead += ConsumeCount;
	CompactPresentationMessageQueueIfNeeded();
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Presentation messages consumed. ConsumeCount=%d RemainingQueueCount=%d"), ConsumeCount, GetPresentationMessageQueueCount());
	CheckPresentationMessageQueueThreshold(TEXT("ConsumePresentationMessages"));
	return Messages;
}

void UCommonQTEManagerComponent::SetPresentationDrainConfig(FCommonQTEPresentationDrainConfig InConfig)
{
	InConfig.MaxMessagesPerDrain = FMath::Max(0, InConfig.MaxMessagesPerDrain);
	InConfig.DrainInterval = FMath::Max(0.0f, InConfig.DrainInterval);
	PresentationDrainConfig = InConfig;
	UE_LOG(
		LogCommonQTE,
		Log,
		TEXT("Presentation drain config changed. MaxMessagesPerDrain=%d DrainInterval=%.3f bAutoDrain=%s"),
		PresentationDrainConfig.MaxMessagesPerDrain,
		PresentationDrainConfig.DrainInterval,
		FCommonQTELog::BoolToString(PresentationDrainConfig.bAutoDrain));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationDrainTimerHandle);
	}
	bPresentationDrainScheduled = false;

	if (!PresentationDrainConfig.bAutoDrain)
	{
		return;
	}

	const FCommonQTEPresentationMessage* HeadMessage = PeekPresentationMessage();
	if (PresentationDrainConfig.MaxMessagesPerDrain <= 0 && HeadMessage != nullptr && !IsInitialPresentationMessage(*HeadMessage))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Presentation auto drain is paused for non-initial messages because MaxMessagesPerDrain is not positive."));
		return;
	}

	SchedulePresentationDrain();
}

FCommonQTEPresentationDrainConfig UCommonQTEManagerComponent::GetPresentationDrainConfig() const
{
	return PresentationDrainConfig;
}

void UCommonQTEManagerComponent::SetMaxPresentationMessagesPerDrain(int32 InMaxMessagesPerDrain)
{
	FCommonQTEPresentationDrainConfig NewConfig = PresentationDrainConfig;
	NewConfig.MaxMessagesPerDrain = InMaxMessagesPerDrain;
	SetPresentationDrainConfig(NewConfig);
}

void UCommonQTEManagerComponent::SetPresentationDrainInterval(float InDrainInterval)
{
	FCommonQTEPresentationDrainConfig NewConfig = PresentationDrainConfig;
	NewConfig.DrainInterval = InDrainInterval;
	SetPresentationDrainConfig(NewConfig);
}

void UCommonQTEManagerComponent::SetAutoDrainPresentationMessages(bool bInAutoDrain)
{
	FCommonQTEPresentationDrainConfig NewConfig = PresentationDrainConfig;
	NewConfig.bAutoDrain = bInAutoDrain;
	SetPresentationDrainConfig(NewConfig);
}

void UCommonQTEManagerComponent::ScheduleVerificationDrain()
{
	if (bVerificationDrainScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UCommonQTEManagerComponent::DrainVerificationQueues);
	VerificationDrainTimerHandle = World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	bVerificationDrainScheduled = true;
}

void UCommonQTEManagerComponent::SchedulePresentationDrain()
{
	if (bPresentationDrainScheduled || !PresentationDrainConfig.bAutoDrain || IsPresentationMessageQueueEmpty())
	{
		return;
	}

	const FCommonQTEPresentationMessage* HeadMessage = PeekPresentationMessage();
	const bool bNextMessageIsInitial = HeadMessage != nullptr && IsInitialPresentationMessage(*HeadMessage);
	if (!bNextMessageIsInitial && PresentationDrainConfig.MaxMessagesPerDrain <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UCommonQTEManagerComponent::DrainPresentationMessages);
	if (bNextMessageIsInitial || PresentationDrainConfig.DrainInterval <= 0.0f)
	{
		PresentationDrainTimerHandle = World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	}
	else
	{
		World->GetTimerManager().SetTimer(PresentationDrainTimerHandle, TimerDelegate, PresentationDrainConfig.DrainInterval, false);
	}
	bPresentationDrainScheduled = true;
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Presentation drain scheduled. QueueCount=%d DrainInterval=%.3f bInitialDrain=%s"), GetPresentationMessageQueueCount(), PresentationDrainConfig.DrainInterval, FCommonQTELog::BoolToString(bNextMessageIsInitial));
}

void UCommonQTEManagerComponent::DrainPresentationMessages()
{
	bPresentationDrainScheduled = false;

	const TArray<FCommonQTEPresentationMessage> Messages = ConsumePresentationMessagesForDrain();
	if (!Messages.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("Presentation messages broadcast. Count=%d RemainingQueueCount=%d"), Messages.Num(), GetPresentationMessageQueueCount());
		const int32 QueueCountBeforeBroadcast = GetPresentationMessageQueueCount();
		const int32 MaxMessagesBeforeBroadcast = PresentationDrainConfig.MaxMessagesPerDrain;
		const float DrainIntervalBeforeBroadcast = PresentationDrainConfig.DrainInterval;
		const bool bAutoDrainBeforeBroadcast = PresentationDrainConfig.bAutoDrain;
		const bool bPresentationDrainScheduledBeforeBroadcast = bPresentationDrainScheduled;
		OnPresentationMessagesReady.Broadcast(Messages);
		if (!ensureAlwaysMsgf(
				GetPresentationMessageQueueCount() == QueueCountBeforeBroadcast
					&& PresentationDrainConfig.MaxMessagesPerDrain == MaxMessagesBeforeBroadcast
					&& FMath::IsNearlyEqual(PresentationDrainConfig.DrainInterval, DrainIntervalBeforeBroadcast)
					&& PresentationDrainConfig.bAutoDrain == bAutoDrainBeforeBroadcast
					&& bPresentationDrainScheduled == bPresentationDrainScheduledBeforeBroadcast,
				TEXT("CommonQTE presentation listeners must not mutate manager presentation state during OnPresentationMessagesReady. QueueBefore=%d QueueAfter=%d MaxBefore=%d MaxAfter=%d IntervalBefore=%.3f IntervalAfter=%.3f AutoBefore=%s AutoAfter=%s ScheduledBefore=%s ScheduledAfter=%s"),
				QueueCountBeforeBroadcast,
				GetPresentationMessageQueueCount(),
				MaxMessagesBeforeBroadcast,
				PresentationDrainConfig.MaxMessagesPerDrain,
				DrainIntervalBeforeBroadcast,
				PresentationDrainConfig.DrainInterval,
				FCommonQTELog::BoolToString(bAutoDrainBeforeBroadcast),
				FCommonQTELog::BoolToString(PresentationDrainConfig.bAutoDrain),
				FCommonQTELog::BoolToString(bPresentationDrainScheduledBeforeBroadcast),
				FCommonQTELog::BoolToString(bPresentationDrainScheduled)))
		{
			return;
		}
	}

	SchedulePresentationDrain();
}

TArray<FCommonQTEPresentationMessage> UCommonQTEManagerComponent::ConsumePresentationMessagesForDrain()
{
	return ConsumePresentationMessages(PresentationDrainConfig.MaxMessagesPerDrain);
}

TArray<FCommonQTEPresentationMessage> UCommonQTEManagerComponent::ConsumeInitialPresentationBatch()
{
	if (IsPresentationMessageQueueEmpty())
	{
		return TArray<FCommonQTEPresentationMessage>();
	}

	const FCommonQTEPresentationMessage InitialBatchHead = PresentationMessageQueue[PresentationMessageQueueHead];
	int32 ConsumeCount = 1;
	while (ConsumeCount < GetPresentationMessageQueueCount() && IsSameInitialPresentationBatch(InitialBatchHead, PresentationMessageQueue[PresentationMessageQueueHead + ConsumeCount]))
	{
		++ConsumeCount;
	}

	TArray<FCommonQTEPresentationMessage> Messages;
	Messages.Reserve(ConsumeCount);
	for (int32 MessageIndex = 0; MessageIndex < ConsumeCount; ++MessageIndex)
	{
		Messages.Add(PresentationMessageQueue[PresentationMessageQueueHead + MessageIndex]);
	}
	PresentationMessageQueueHead += ConsumeCount;
	CompactPresentationMessageQueueIfNeeded();
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Initial presentation messages consumed for drain. ConsumeCount=%d RemainingQueueCount=%d"), ConsumeCount, GetPresentationMessageQueueCount());
	CheckPresentationMessageQueueThreshold(TEXT("ConsumeInitialPresentationBatch"));
	return Messages;
}

bool UCommonQTEManagerComponent::IsInitialPresentationMessage(const FCommonQTEPresentationMessage& Message) const
{
	return Message.Phase == ECommonQTEPresentationPhase::InitialState;
}

bool UCommonQTEManagerComponent::IsSameInitialPresentationBatch(const FCommonQTEPresentationMessage& Left, const FCommonQTEPresentationMessage& Right) const
{
	return IsInitialPresentationMessage(Left)
		&& IsInitialPresentationMessage(Right)
		&& Left.WholeHandle == Right.WholeHandle
		&& Left.Source == Right.Source;
}

bool UCommonQTEManagerComponent::IsPresentationMessageQueueEmpty() const
{
	return GetPresentationMessageQueueCount() <= 0;
}

int32 UCommonQTEManagerComponent::GetPresentationMessageQueueCount() const
{
	return FMath::Max(0, PresentationMessageQueue.Num() - PresentationMessageQueueHead);
}

const FCommonQTEPresentationMessage* UCommonQTEManagerComponent::PeekPresentationMessage() const
{
	return PresentationMessageQueue.IsValidIndex(PresentationMessageQueueHead) ? &PresentationMessageQueue[PresentationMessageQueueHead] : nullptr;
}

void UCommonQTEManagerComponent::CompactPresentationMessageQueueIfNeeded()
{
	if (PresentationMessageQueueHead <= 0)
	{
		return;
	}

	if (PresentationMessageQueueHead >= PresentationMessageQueue.Num())
	{
		PresentationMessageQueue.Reset();
		PresentationMessageQueueHead = 0;
		return;
	}

	if (PresentationMessageQueueHead >= 32 && PresentationMessageQueueHead * 2 >= PresentationMessageQueue.Num())
	{
		PresentationMessageQueue.RemoveAt(0, PresentationMessageQueueHead, EAllowShrinking::No);
		PresentationMessageQueueHead = 0;
	}
}

void UCommonQTEManagerComponent::DrainVerificationQueues()
{
	bVerificationDrainScheduled = false;

	TArray<TWeakObjectPtr<ACommonQTEPerformerActor>> DirtyPerformers = DirtyVerificationPerformers;
	DirtyVerificationPerformers.Reset();
	UE_LOG(LogCommonQTE, Verbose, TEXT("Verification drain started. DirtyPerformerCount=%d"), DirtyPerformers.Num());

	for (const TWeakObjectPtr<ACommonQTEPerformerActor>& DirtyPerformer : DirtyPerformers)
	{
		ACommonQTEPerformerActor* Performer = DirtyPerformer.Get();
		if (!IsValid(Performer))
		{
			continue;
		}

		const TArray<FCommonQTEPerformerPredictionMessage> Messages = Performer->ConsumePendingVerificationMessages();
		UE_LOG(LogCommonQTE, Verbose, TEXT("Verification messages consumed. Performer=%s MessageCount=%d"), *GetNameSafe(Performer), Messages.Num());
		for (const FCommonQTEPerformerPredictionMessage& Message : Messages)
		{
			HandlePerformerVerification(Performer, Message);
		}
	}
}

bool UCommonQTEManagerComponent::IsPerformerRegisteredForEntity(const TSharedPtr<FCommonQTEEntity>& Entity, ACommonQTEPerformerActor* Performer) const
{
	if (!Entity.IsValid() || !IsValid(Performer))
	{
		return false;
	}

	for (ACommonQTEPerformerActor* RegisteredPerformer : Entity->GetDriver().GetReplicationProxy().GetValidPerformers())
	{
		if (RegisteredPerformer == Performer)
		{
			return true;
		}
	}
	return false;
}

bool UCommonQTEManagerComponent::CanVerifyPredictionMessage(ACommonQTEPerformerActor* Performer, const FCommonQTEEntity& Entity, const FCommonQTEPerformerPredictionMessage& Message) const
{
	if (!IsValid(Performer) || !Message.WholeHandle.IsValid())
	{
		return false;
	}

	return Message.CellIndex == Entity.GetDriver().GetCellIndex();
}

void UCommonQTEManagerComponent::MarkPredictionMessageResolved(ACommonQTEPerformerActor* Performer, int32 PredictionId)
{
	if (IsValid(Performer) && PredictionId > 0)
	{
		int32& LastResolvedPredictionId = LastResolvedPredictionIdMap.FindOrAdd(TObjectKey<ACommonQTEPerformerActor>(Performer));
		LastResolvedPredictionId = FMath::Max(LastResolvedPredictionId, PredictionId);
	}
}

void UCommonQTEManagerComponent::EnqueueRollBackToPerformer(ACommonQTEPerformerActor* Performer, const FCommonQTEHandle& Handle, int32 PredictionId, int32 RejectedCellIndex, const FCommonQTEStateSnapshot& AuthoritativeStateSnapshot)
{
	if (!IsValid(Performer))
	{
		return;
	}

	FCommonQTEPerformerRollBackMessage RollBackMessage;
	RollBackMessage.WholeHandle = Handle;
	RollBackMessage.PredictionId = PredictionId;
	RollBackMessage.RejectedCellIndex = RejectedCellIndex;
	RollBackMessage.AuthoritativeStateSnapshot = FCommonQTEStateRuntime::BuildCompactStateSnapshot(AuthoritativeStateSnapshot);
	Performer->EnqueueRollBackMessage(RollBackMessage);
	Performer->FlushRollBackMessagesToClient();
	UE_LOG(LogCommonQTE, Log, TEXT("Rollback produced. Handle=%d PredictionId=%d RejectedCellIndex=%d Performer=%s"), FCommonQTELog::HandleValue(Handle), PredictionId, RejectedCellIndex, *GetNameSafe(Performer));
}

void UCommonQTEManagerComponent::HandlePerformerVerification(ACommonQTEPerformerActor* Performer, const FCommonQTEPerformerPredictionMessage& Message)
{
	if (!IsAuthority() || FinalizedEntities.Contains(Message.WholeHandle))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("HandlePerformerVerification rejected. Handle=%d PredictionId=%d bAuthority=%s bFinalized=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, FCommonQTELog::BoolToString(IsAuthority()), FCommonQTELog::BoolToString(FinalizedEntities.Contains(Message.WholeHandle)));
		return;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Message.WholeHandle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("HandlePerformerVerification failed because entity is missing. Handle=%d PredictionId=%d"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId);
		return;
	}

	if (!IsPerformerRegisteredForEntity(Entity, Performer))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("HandlePerformerVerification rejected because performer is not registered for entity. Handle=%d PredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, *GetNameSafe(Performer));
		return;
	}

	if (Message.PredictionId <= 0)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("HandlePerformerVerification rejected invalid prediction id. Handle=%d PredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, *GetNameSafe(Performer));
		return;
	}

	const int32* LastResolvedPredictionId = LastResolvedPredictionIdMap.Find(TObjectKey<ACommonQTEPerformerActor>(Performer));
	if (LastResolvedPredictionId != nullptr && Message.PredictionId <= *LastResolvedPredictionId)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("HandlePerformerVerification ignored stale prediction. Handle=%d PredictionId=%d LastResolvedPredictionId=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, *LastResolvedPredictionId, *GetNameSafe(Performer));
		return;
	}

	if (!CanVerifyPredictionMessage(Performer, *Entity, Message))
	{
		FCommonQTEStateSnapshot AuthoritativeStateSnapshot;
		Entity->BuildStateSnapshot(AuthoritativeStateSnapshot);
		MarkPredictionMessageResolved(Performer, Message.PredictionId);
		EnqueueRollBackToPerformer(Performer, Message.WholeHandle, Message.PredictionId, Message.CellIndex, AuthoritativeStateSnapshot);
		UE_LOG(LogCommonQTE, Warning, TEXT("HandlePerformerVerification rejected prediction before applying input. Handle=%d PredictionId=%d MessageCellIndex=%d AuthorityCellIndex=%d Performer=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), Message.PredictionId, Message.CellIndex, Entity->GetDriver().GetCellIndex(), *GetNameSafe(Performer));
		return;
	}

	const FCommonQTEApplyInputResult ApplyResult = Entity->HandleInputWithResult(Message.CellContent);
	MarkPredictionMessageResolved(Performer, Message.PredictionId);
	UE_LOG(
		LogCommonQTE,
		Log,
		TEXT("Prediction verified. Handle=%d PredictionId=%d InputCellIndex=%d AppliedCellIndex=%d CellContent=%d bAccepted=%s bMatched=%s State=%s"),
		FCommonQTELog::HandleValue(Message.WholeHandle),
		Message.PredictionId,
		Message.CellIndex,
		ApplyResult.AppliedCellIndex,
		Message.CellContent,
		FCommonQTELog::BoolToString(ApplyResult.bAccepted),
		FCommonQTELog::BoolToString(ApplyResult.bMatched),
		FCommonQTELog::StateToString(Entity->GetState()));
	if (ApplyResult.bAccepted)
	{
		UpdateObserverStateDelta(Message.WholeHandle, ApplyResult.AppliedCellIndex);
	}

	TWeakObjectPtr<ACommonQTEPerformerActor> WeakPerformer = Performer;
	OnPerformerVerificationApplied.Broadcast(Performer, Message, ApplyResult);
	const TSharedPtr<FCommonQTEEntity> CurrentEntity = FindEntity(Message.WholeHandle);
	if (!ensureAlwaysMsgf(
			IsValid(WeakPerformer.Get()) && CurrentEntity == Entity,
			TEXT("CommonQTE verification listeners must not destroy the performer or remove/reset the entity during OnPerformerVerificationApplied. Handle=%d PredictionId=%d bPerformerValid=%s bEntityUnchanged=%s"),
			FCommonQTELog::HandleValue(Message.WholeHandle),
			Message.PredictionId,
			FCommonQTELog::BoolToString(IsValid(WeakPerformer.Get())),
			FCommonQTELog::BoolToString(CurrentEntity == Entity)))
	{
		return;
	}

	if ((!ApplyResult.bAccepted || !ApplyResult.bMatched) && Performer != nullptr)
	{
		FCommonQTEStateSnapshot AuthoritativeStateSnapshot;
		Entity->BuildStateSnapshot(AuthoritativeStateSnapshot);
		EnqueueRollBackToPerformer(Performer, Message.WholeHandle, Message.PredictionId, ApplyResult.AppliedCellIndex != INDEX_NONE ? ApplyResult.AppliedCellIndex : Message.CellIndex, AuthoritativeStateSnapshot);
	}
	else if (Performer != nullptr)
	{
		Performer->AcknowledgePredictionMessage(Message.PredictionId);
	}

	if (IsTerminalState(Entity->GetState()))
	{
		UE_LOG(LogCommonQTE, Log, TEXT("Entity reached terminal state during verification. Handle=%d State=%s"), FCommonQTELog::HandleValue(Message.WholeHandle), FCommonQTELog::StateToString(Entity->GetState()));
		FinalizeEntity(Message.WholeHandle);
	}
}

void UCommonQTEManagerComponent::UpdateInitialStateSnapshot(const FCommonQTEHandle& Handle)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		return;
	}

	ACommonQTEObserverActor* Observer = Entity->GetDriver().GetReplicationProxy().GetObserver();
	if (Observer == nullptr)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("UpdateInitialStateSnapshot skipped because observer is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return;
	}

	FCommonQTEStateSnapshot StateSnapshot;
	Entity->BuildStateSnapshot(StateSnapshot);
	Observer->SetInitialStateSnapshot(StateSnapshot);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer initial state snapshot updated. Handle=%d CellCount=%d"), FCommonQTELog::HandleValue(Handle), StateSnapshot.CellContents.Num());
}

void UCommonQTEManagerComponent::UpdateObserverExcludedPlayerControllers(const FCommonQTEHandle& Handle)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		return;
	}

	ACommonQTEObserverActor* Observer = Entity->GetDriver().GetReplicationProxy().GetObserver();
	if (Observer == nullptr)
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("UpdateObserverExcludedPlayerControllers skipped because observer is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return;
	}

	TArray<APlayerController*> ExcludedPlayerControllers;
	for (ACommonQTEPerformerActor* Performer : Entity->GetDriver().GetReplicationProxy().GetValidPerformers())
	{
		APlayerController* PlayerController = IsValid(Performer) ? Cast<APlayerController>(Performer->GetOwner()) : nullptr;
		if (PlayerController != nullptr)
		{
			ExcludedPlayerControllers.Add(PlayerController);
		}
	}

	Observer->SetExcludedPlayerControllers(ExcludedPlayerControllers);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer excluded player controllers updated. Handle=%d ExcludedCount=%d"), FCommonQTELog::HandleValue(Handle), ExcludedPlayerControllers.Num());
}

void UCommonQTEManagerComponent::UpdateObserverStateDelta(const FCommonQTEHandle& Handle, int32 ChangedCellIndex)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		return;
	}

	ACommonQTEObserverActor* Observer = Entity->GetDriver().GetReplicationProxy().GetObserver();
	if (Observer == nullptr)
	{
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("UpdateObserverStateDelta skipped because observer is missing. Handle=%d ChangedCellIndex=%d"), FCommonQTELog::HandleValue(Handle), ChangedCellIndex);
		return;
	}

	FCommonQTEObserverStateDelta StateDelta;
	Entity->BuildObserverStateDelta(ChangedCellIndex, AllocateObserverSequenceId(Handle), StateDelta);
	FCommonQTEStateSnapshot StateSnapshot;
	Entity->BuildStateSnapshot(StateSnapshot);
	Observer->PushStateSnapshot(StateSnapshot, StateDelta);
	UE_LOG(LogCommonQTE, Verbose, TEXT("Observer state delta pushed. Handle=%d SequenceId=%d ChangedCellIndex=%d"), FCommonQTELog::HandleValue(Handle), StateDelta.SequenceId, ChangedCellIndex);
}

void UCommonQTEManagerComponent::SyncPerformersToAuthoritativeState(const FCommonQTEHandle& Handle, int32 PredictionId, int32 RejectedCellIndex)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		return;
	}

	FCommonQTEStateSnapshot StateSnapshot;
	Entity->BuildStateSnapshot(StateSnapshot);

	int32 SyncedPerformerCount = 0;
	for (ACommonQTEPerformerActor* Performer : Entity->GetDriver().GetReplicationProxy().GetValidPerformers())
	{
		if (!IsValid(Performer))
		{
			continue;
		}

		EnqueueRollBackToPerformer(Performer, Handle, PredictionId, RejectedCellIndex, StateSnapshot);
		++SyncedPerformerCount;
	}
	UE_LOG(LogCommonQTE, Log, TEXT("Performers synced to authoritative state. Handle=%d PredictionId=%d RejectedCellIndex=%d PerformerCount=%d State=%s"), FCommonQTELog::HandleValue(Handle), PredictionId, RejectedCellIndex, SyncedPerformerCount, FCommonQTELog::StateToString(StateSnapshot.QTEState));
}

bool UCommonQTEManagerComponent::IsAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

bool UCommonQTEManagerComponent::IsTerminalState(ECommonQTEState State) const
{
	return State == ECommonQTEState::Success || State == ECommonQTEState::Fail;
}

void UCommonQTEManagerComponent::FinalizeEntity(const FCommonQTEHandle& Handle)
{
	if (FinalizedEntities.Contains(Handle))
	{
		UE_LOG(LogCommonQTE, Verbose, TEXT("FinalizeEntity skipped because entity is already finalized. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return;
	}

	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("FinalizeEntity failed because entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return;
	}

	FinalizedEntities.Add(Handle);
	ClearLimitTimer(Handle);
	SyncPerformersToAuthoritativeState(Handle, 0, INDEX_NONE);
	UE_LOG(LogCommonQTE, Log, TEXT("Entity finalized. Handle=%d State=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::StateToString(Entity->GetState()));

	const float CleanupDelay = FMath::Max(FinishedEntityCleanupDelay, 0.0f);
	for (ACommonQTEPerformerActor* Performer : Entity->GetDriver().GetReplicationProxy().GetValidPerformers())
	{
		if (IsValid(Performer))
		{
			Performer->SetLifeSpan(CleanupDelay + 0.5f);
		}
	}

	if (ACommonQTEObserverActor* Observer = Entity->GetDriver().GetReplicationProxy().GetObserver())
	{
		Observer->SetLifeSpan(CleanupDelay + 0.5f);
	}

	ScheduleEntityCleanup(Handle);
}

void UCommonQTEManagerComponent::ScheduleEntityCleanup(const FCommonQTEHandle& Handle)
{
	ClearCleanupTimer(Handle);

	if (FinishedEntityCleanupDelay <= 0.0f)
	{
		UE_LOG(LogCommonQTE, Log, TEXT("Entity cleanup executes immediately. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		RemoveEntity(Handle);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, &UCommonQTEManagerComponent::HandleCleanupTimerExpired, Handle);
		World->GetTimerManager().SetTimer(EntityCleanupTimers.FindOrAdd(Handle), TimerDelegate, FinishedEntityCleanupDelay, false);
		UE_LOG(LogCommonQTE, Log, TEXT("Entity cleanup scheduled. Handle=%d CleanupDelay=%.3f"), FCommonQTELog::HandleValue(Handle), FinishedEntityCleanupDelay);
	}
}

void UCommonQTEManagerComponent::HandleCleanupTimerExpired(FCommonQTEHandle Handle)
{
	UE_LOG(LogCommonQTE, Log, TEXT("Entity cleanup timer expired. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	RemoveEntity(Handle);
}

void UCommonQTEManagerComponent::HandleLimitTimeExpired(FCommonQTEHandle Handle)
{
	TSharedPtr<FCommonQTEEntity> Entity = FindEntity(Handle);
	if (!Entity.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("Limit timer expired but entity is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return;
	}

	Entity->Fail();
	UpdateObserverStateDelta(Handle, INDEX_NONE);
	UE_LOG(LogCommonQTE, Log, TEXT("Limit timer expired. Handle=%d State=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::StateToString(Entity->GetState()));
	FinalizeEntity(Handle);
}

void UCommonQTEManagerComponent::ClearLimitTimer(const FCommonQTEHandle& Handle)
{
	if (FTimerHandle* TimerHandle = EntityTimers.Find(Handle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		EntityTimers.Remove(Handle);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Limit timer cleared. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	}
}

void UCommonQTEManagerComponent::ClearCleanupTimer(const FCommonQTEHandle& Handle)
{
	if (FTimerHandle* TimerHandle = EntityCleanupTimers.Find(Handle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		EntityCleanupTimers.Remove(Handle);
		UE_LOG(LogCommonQTE, VeryVerbose, TEXT("Cleanup timer cleared. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	}
}

int32 UCommonQTEManagerComponent::AllocateObserverSequenceId(const FCommonQTEHandle& Handle)
{
	int32& SequenceId = ObserverSequenceIdMap.FindOrAdd(Handle);
	++SequenceId;
	return SequenceId;
}

void UCommonQTEManagerComponent::CheckPresentationMessageQueueThreshold(const TCHAR* Context)
{
	const UCommonQTESettings* Settings = GetDefault<UCommonQTESettings>();
	const int32 Threshold = Settings != nullptr ? Settings->PresentationMessageQueueWarningThreshold : 100;
	if (Threshold <= 0)
	{
		bPresentationQueueThresholdReported = false;
		return;
	}

	const int32 QueueCount = GetPresentationMessageQueueCount();
	if (QueueCount <= Threshold)
	{
		bPresentationQueueThresholdReported = false;
		return;
	}

	if (bPresentationQueueThresholdReported)
	{
		return;
	}

	bPresentationQueueThresholdReported = true;
	const TCHAR* SafeContext = Context != nullptr ? Context : TEXT("Unknown");
	ensureAlwaysMsgf(false, TEXT("CommonQTE presentation message queue exceeded warning threshold. Context=%s QueueCount=%d Threshold=%d"), SafeContext, QueueCount, Threshold);
	UE_LOG(LogCommonQTE, Error, TEXT("Presentation message queue exceeded warning threshold. Context=%s QueueCount=%d Threshold=%d"), SafeContext, QueueCount, Threshold);
}

APlayerController* UCommonQTEManagerComponent::ResolvePlayerController(AActor* PerformerOwner) const
{
	if (PerformerOwner == nullptr)
	{
		return nullptr;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(PerformerOwner))
	{
		return PlayerController;
	}

	if (APawn* Pawn = Cast<APawn>(PerformerOwner))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	return Cast<APlayerController>(PerformerOwner->GetInstigatorController());
}
