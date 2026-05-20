#pragma once

#include "CoreMinimal.h"
#include "CommonQTEHandle.h"

#include "CommonQTEType.generated.h"

class FCommonQTEEntity;

UENUM(BlueprintType)
enum class ECommonQTEState : uint8
{
	OnGoing,
	Pause,
	Success,
	Fail,
};

UENUM(BlueprintType)
enum class ECommonQTECellState : uint8
{
	OnGoing,
	Success,
	Fail,
};

UENUM(BlueprintType)
enum class ECommonQTEIndexChangeTypeWhenErrorOccur : uint8
{
	Stay,
	ResetToInitial,
	MoveToNext,
};

UENUM(BlueprintType)
enum class ECommonQTEPresentationSource : uint8
{
	Observer,
	Performer,
};

UENUM(BlueprintType)
enum class ECommonQTEPresentationPhase : uint8
{
	InitialState,
	StateDelta,
	Prediction,
	Rollback,
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTERule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	float LimitTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE", meta = (ClampMin = "0", DisplayName = "Max Tolerable Error Count", ToolTip = "Maximum number of wrong inputs the QTE can tolerate before the next wrong input fails it. 0 means the first wrong input fails the QTE."))
	int32 MaxTolerableErrorCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEIndexChangeTypeWhenErrorOccur IndexChangeTypeWhenErrorOccur = ECommonQTEIndexChangeTypeWhenErrorOccur::Stay;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTECellGenerateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 InputTypeCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	bool bUseRandomSeed = false;
};

struct COMMONQTE_CORE_API FCommonQTECellOwnerEntityInfo
{
	TWeakPtr<FCommonQTEEntity> OwnerEntity;

	int32 CellIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEDriverStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTERule Rule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 ErrorCount = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEDriverStateSnapshot DriverState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEState QTEState = ECommonQTEState::OnGoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	TArray<int32> CellContents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	TArray<ECommonQTECellState> CellStates;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTECompactStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 ErrorCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEState QTEState = ECommonQTEState::OnGoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	TArray<ECommonQTECellState> CellStates;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEPerformerPredictionMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 PredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellContent = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEPerformerRollBackMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 PredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 RejectedCellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTECompactStateSnapshot AuthoritativeStateSnapshot;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEPresentationMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEPresentationSource Source = ECommonQTEPresentationSource::Observer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEPresentationPhase Phase = ECommonQTEPresentationPhase::StateDelta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 SequenceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 PredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEState QTEState = ECommonQTEState::OnGoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTECellState CellState = ECommonQTECellState::OnGoing;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonQTEPresentationMessagesReady, const TArray<FCommonQTEPresentationMessage>&, Messages);

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEPresentationDrainConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 MaxMessagesPerDrain = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	float DrainInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	bool bAutoDrain = true;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEPerformerPredictionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 PredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEPerformerPredictionMessage PredictionMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEStateSnapshot StateBeforePrediction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEStateSnapshot StateAfterPrediction;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEApplyInputResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	bool bAccepted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	bool bMatched = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 AppliedCellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEStateSnapshot StateBeforeInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEStateSnapshot StateAfterInput;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEObserverStateDelta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTEHandle WholeHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 SequenceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 CellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTEState QTEState = ECommonQTEState::OnGoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	ECommonQTECellState CellState = ECommonQTECellState::OnGoing;
};

USTRUCT(BlueprintType)
struct COMMONQTE_CORE_API FCommonQTEObserverReplicatedState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	FCommonQTECompactStateSnapshot StateSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE")
	int32 SequenceId = 0;
};
