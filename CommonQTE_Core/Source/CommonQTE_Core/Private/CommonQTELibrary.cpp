#include "CommonQTELibrary.h"

#include "CommonQTELog.h"
#include "CommonQTEManagerComponent.h"
#include "CommonQTEObserverActor.h"
#include "CommonQTEPerformerActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

FCommonQTEHandle UCommonQTELibrary::StartCommonQTE(const UObject* WorldContextObject, const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const TArray<AActor*>& PerformerOwners, bool bCreateObserver)
{
	return StartCommonQTEWithActorClasses(WorldContextObject, Rule, GenerateConfig, PerformerOwners, bCreateObserver, FCommonQTEActorClassConfig());
}

FCommonQTEHandle UCommonQTELibrary::StartCommonQTEWithActorClasses(const UObject* WorldContextObject, const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const TArray<AActor*>& PerformerOwners, bool bCreateObserver, const FCommonQTEActorClassConfig& ActorClassConfig)
{
	UE_LOG(
		LogCommonQTE,
		Log,
		TEXT("StartCommonQTE requested. InputTypeCount=%d CellCount=%d bUseRandomSeed=%s RandomSeed=%d PerformerOwnerCount=%d bCreateObserver=%s LimitTime=%.3f MaxTolerableErrorCount=%d ErrorIndexChange=%s RuntimePerformerClass=%s RuntimeObserverClass=%s"),
		GenerateConfig.InputTypeCount,
		GenerateConfig.CellCount,
		FCommonQTELog::BoolToString(GenerateConfig.bUseRandomSeed),
		GenerateConfig.RandomSeed,
		PerformerOwners.Num(),
		FCommonQTELog::BoolToString(bCreateObserver),
		Rule.LimitTime,
		Rule.MaxTolerableErrorCount,
		FCommonQTELog::IndexChangeTypeToString(Rule.IndexChangeTypeWhenErrorOccur),
		*GetNameSafe(ActorClassConfig.PerformerActorClass.Get()),
		*GetNameSafe(ActorClassConfig.ObserverActorClass.Get()));

	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, true);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StartCommonQTE failed because manager is missing."));
		return FCommonQTEHandle();
	}

	const FCommonQTEHandle Handle = Manager->CreateEntity(Rule, GenerateConfig, ActorClassConfig);
	if (!Handle.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StartCommonQTE failed because entity creation returned invalid handle."));
		return Handle;
	}

	SetupCommonQTEActors(Manager, Handle, PerformerOwners, bCreateObserver);
	UE_LOG(LogCommonQTE, Log, TEXT("StartCommonQTE completed. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	return Handle;
}

FCommonQTEHandle UCommonQTELibrary::StartFixedCommonQTE(const UObject* WorldContextObject, const FCommonQTERule& Rule, const TArray<int32>& CellContents, const TArray<AActor*>& PerformerOwners, bool bCreateObserver)
{
	return StartFixedCommonQTEWithActorClasses(WorldContextObject, Rule, CellContents, PerformerOwners, bCreateObserver, FCommonQTEActorClassConfig());
}

FCommonQTEHandle UCommonQTELibrary::StartFixedCommonQTEWithActorClasses(const UObject* WorldContextObject, const FCommonQTERule& Rule, const TArray<int32>& CellContents, const TArray<AActor*>& PerformerOwners, bool bCreateObserver, const FCommonQTEActorClassConfig& ActorClassConfig)
{
	UE_LOG(
		LogCommonQTE,
		Log,
		TEXT("StartFixedCommonQTE requested. CellCount=%d PerformerOwnerCount=%d bCreateObserver=%s LimitTime=%.3f MaxTolerableErrorCount=%d ErrorIndexChange=%s RuntimePerformerClass=%s RuntimeObserverClass=%s"),
		CellContents.Num(),
		PerformerOwners.Num(),
		FCommonQTELog::BoolToString(bCreateObserver),
		Rule.LimitTime,
		Rule.MaxTolerableErrorCount,
		FCommonQTELog::IndexChangeTypeToString(Rule.IndexChangeTypeWhenErrorOccur),
		*GetNameSafe(ActorClassConfig.PerformerActorClass.Get()),
		*GetNameSafe(ActorClassConfig.ObserverActorClass.Get()));
	UE_LOG(LogCommonQTE, VeryVerbose, TEXT("StartFixedCommonQTE cell contents: %s"), *FCommonQTELog::CellContentsToString(CellContents));
	if (CellContents.IsEmpty())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StartFixedCommonQTE received empty CellContents."));
	}

	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, true);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StartFixedCommonQTE failed because manager is missing."));
		return FCommonQTEHandle();
	}

	const FCommonQTEHandle Handle = Manager->CreateEntityWithFixedCells(Rule, CellContents, ActorClassConfig);
	if (!Handle.IsValid())
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StartFixedCommonQTE failed because entity creation returned invalid handle."));
		return Handle;
	}

	SetupCommonQTEActors(Manager, Handle, PerformerOwners, bCreateObserver);
	UE_LOG(LogCommonQTE, Log, TEXT("StartFixedCommonQTE completed. Handle=%d"), FCommonQTELog::HandleValue(Handle));
	return Handle;
}

ACommonQTEPerformerActor* UCommonQTELibrary::AddCommonQTEPerformer(const UObject* WorldContextObject, FCommonQTEHandle Handle, AActor* PerformerOwner)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("AddCommonQTEPerformer failed because manager is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return nullptr;
	}

	ACommonQTEPerformerActor* Performer = Manager->AddPerformer(Handle, PerformerOwner);
	UE_LOG(LogCommonQTE, Log, TEXT("AddCommonQTEPerformer completed. Handle=%d Performer=%s"), FCommonQTELog::HandleValue(Handle), *GetNameSafe(Performer));
	return Performer;
}

FCommonQTEPerformerPredictionMessage UCommonQTELibrary::SubmitCommonQTEPrediction(ACommonQTEPerformerActor* Performer, int32 CellContent)
{
	if (!IsValid(Performer))
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("SubmitCommonQTEPrediction failed because performer is invalid. CellContent=%d"), CellContent);
		return FCommonQTEPerformerPredictionMessage();
	}

	return Performer->SubmitPrediction(CellContent);
}

bool UCommonQTELibrary::StopCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("StopCommonQTE failed because manager is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	const bool bStopped = Manager->StopEntity(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("StopCommonQTE completed. Handle=%d bStopped=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(bStopped));
	return bStopped;
}

bool UCommonQTELibrary::PauseCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("PauseCommonQTE failed because manager is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	const bool bPaused = Manager->PauseEntity(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("PauseCommonQTE completed. Handle=%d bPaused=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(bPaused));
	return bPaused;
}

bool UCommonQTELibrary::ResumeCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ResumeCommonQTE failed because manager is missing. Handle=%d"), FCommonQTELog::HandleValue(Handle));
		return false;
	}

	const bool bResumed = Manager->ResumeEntity(Handle);
	UE_LOG(LogCommonQTE, Log, TEXT("ResumeCommonQTE completed. Handle=%d bResumed=%s"), FCommonQTELog::HandleValue(Handle), FCommonQTELog::BoolToString(bResumed));
	return bResumed;
}

int32 UCommonQTELibrary::PauseAllCommonQTE(const UObject* WorldContextObject)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("PauseAllCommonQTE failed because manager is missing."));
		return 0;
	}

	const int32 PausedCount = Manager->PauseAllEntities();
	UE_LOG(LogCommonQTE, Log, TEXT("PauseAllCommonQTE completed. PausedCount=%d"), PausedCount);
	return PausedCount;
}

int32 UCommonQTELibrary::ResumeAllCommonQTE(const UObject* WorldContextObject)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("ResumeAllCommonQTE failed because manager is missing."));
		return 0;
	}

	const int32 ResumedCount = Manager->ResumeAllEntities();
	UE_LOG(LogCommonQTE, Log, TEXT("ResumeAllCommonQTE completed. ResumedCount=%d"), ResumedCount);
	return ResumedCount;
}

ECommonQTEState UCommonQTELibrary::GetCommonQTEState(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	return Manager != nullptr ? Manager->GetEntityState(Handle) : ECommonQTEState::Fail;
}

ECommonQTECellState UCommonQTELibrary::GetCommonQTECellState(const UObject* WorldContextObject, FCommonQTEHandle Handle, int32 CellIndex)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	return Manager != nullptr ? Manager->GetCellState(Handle, CellIndex) : ECommonQTECellState::Fail;
}

TArray<ACommonQTEPerformerActor*> UCommonQTELibrary::GetCommonQTEPerformers(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	return Manager != nullptr ? Manager->GetPerformers(Handle) : TArray<ACommonQTEPerformerActor*>();
}

ACommonQTEObserverActor* UCommonQTELibrary::GetCommonQTEObserver(const UObject* WorldContextObject, FCommonQTEHandle Handle)
{
	UCommonQTEManagerComponent* Manager = GetCommonQTEManager(WorldContextObject, false);
	return Manager != nullptr ? Manager->GetObserver(Handle) : nullptr;
}

UCommonQTEManagerComponent* UCommonQTELibrary::GetCommonQTEManager(const UObject* WorldContextObject, bool bCreateIfMissing)
{
	UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("GetCommonQTEManager failed because GameState is missing. bCreateIfMissing=%s"), FCommonQTELog::BoolToString(bCreateIfMissing));
		return nullptr;
	}

	UCommonQTEManagerComponent* Manager = GameState->FindComponentByClass<UCommonQTEManagerComponent>();
	if (Manager != nullptr || !bCreateIfMissing || !GameState->HasAuthority())
	{
		if (Manager == nullptr && bCreateIfMissing && !GameState->HasAuthority())
		{
			UE_LOG(LogCommonQTE, Warning, TEXT("GetCommonQTEManager cannot create manager on non-authority GameState."));
		}
		else
		{
			UE_LOG(LogCommonQTE, VeryVerbose, TEXT("GetCommonQTEManager returned existing or null manager. Manager=%s bCreateIfMissing=%s"), *GetNameSafe(Manager), FCommonQTELog::BoolToString(bCreateIfMissing));
		}
		return Manager;
	}

	Manager = NewObject<UCommonQTEManagerComponent>(GameState);
	if (Manager == nullptr)
	{
		UE_LOG(LogCommonQTE, Warning, TEXT("GetCommonQTEManager failed to allocate manager component."));
		return nullptr;
	}

	Manager->SetIsReplicated(true);
	GameState->AddInstanceComponent(Manager);
	Manager->RegisterComponent();
	UE_LOG(LogCommonQTE, Log, TEXT("GetCommonQTEManager created manager component. Manager=%s"), *GetNameSafe(Manager));
	return Manager;
}

void UCommonQTELibrary::SetupCommonQTEActors(UCommonQTEManagerComponent* Manager, FCommonQTEHandle Handle, const TArray<AActor*>& PerformerOwners, bool bCreateObserver)
{
	if (Manager == nullptr || !Handle.IsValid())
	{
		return;
	}

	for (AActor* PerformerOwner : PerformerOwners)
	{
		Manager->AddPerformer(Handle, PerformerOwner);
	}

	if (bCreateObserver)
	{
		Manager->CreateObserver(Handle);
	}
	UE_LOG(LogCommonQTE, Verbose, TEXT("SetupCommonQTEActors completed. Handle=%d PerformerOwnerCount=%d bCreateObserver=%s"), FCommonQTELog::HandleValue(Handle), PerformerOwners.Num(), FCommonQTELog::BoolToString(bCreateObserver));
}
