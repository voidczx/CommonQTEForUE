#pragma once

#include "CoreMinimal.h"
#include "CommonQTEActorClassConfig.h"
#include "CommonQTEType.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CommonQTELibrary.generated.h"

class ACommonQTEObserverActor;
class ACommonQTEPerformerActor;
class AActor;
class UCommonQTEManagerComponent;

UCLASS()
class COMMONQTE_CORE_API UCommonQTELibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static FCommonQTEHandle StartCommonQTE(const UObject* WorldContextObject, const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const TArray<AActor*>& PerformerOwners, bool bCreateObserver);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static FCommonQTEHandle StartCommonQTEWithActorClasses(const UObject* WorldContextObject, const FCommonQTERule& Rule, const FCommonQTECellGenerateConfig& GenerateConfig, const TArray<AActor*>& PerformerOwners, bool bCreateObserver, const FCommonQTEActorClassConfig& ActorClassConfig);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static FCommonQTEHandle StartFixedCommonQTE(const UObject* WorldContextObject, const FCommonQTERule& Rule, const TArray<int32>& CellContents, const TArray<AActor*>& PerformerOwners, bool bCreateObserver);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static FCommonQTEHandle StartFixedCommonQTEWithActorClasses(const UObject* WorldContextObject, const FCommonQTERule& Rule, const TArray<int32>& CellContents, const TArray<AActor*>& PerformerOwners, bool bCreateObserver, const FCommonQTEActorClassConfig& ActorClassConfig);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static ACommonQTEPerformerActor* AddCommonQTEPerformer(const UObject* WorldContextObject, FCommonQTEHandle Handle, AActor* PerformerOwner);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE")
	static FCommonQTEPerformerPredictionMessage SubmitCommonQTEPrediction(ACommonQTEPerformerActor* Performer, int32 CellContent);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static bool StopCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static bool PauseCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static bool ResumeCommonQTE(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static int32 PauseAllCommonQTE(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static int32 ResumeAllCommonQTE(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static ECommonQTEState GetCommonQTEState(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintPure, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static ECommonQTECellState GetCommonQTECellState(const UObject* WorldContextObject, FCommonQTEHandle Handle, int32 CellIndex);

	UFUNCTION(BlueprintPure, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static TArray<ACommonQTEPerformerActor*> GetCommonQTEPerformers(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintPure, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static ACommonQTEObserverActor* GetCommonQTEObserver(const UObject* WorldContextObject, FCommonQTEHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE", meta = (WorldContext = "WorldContextObject"))
	static UCommonQTEManagerComponent* GetCommonQTEManager(const UObject* WorldContextObject, bool bCreateIfMissing);

private:
	static void SetupCommonQTEActors(UCommonQTEManagerComponent* Manager, FCommonQTEHandle Handle, const TArray<AActor*>& PerformerOwners, bool bCreateObserver);
};
