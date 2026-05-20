#pragma once

#include "CoreMinimal.h"
#include "CommonQTEHandle.h"
#include "CommonQTEType.h"
#include "Templates/SubclassOf.h"

#include "CommonQTETestType.generated.h"

class UCommonQTEFlowTestWidget;

UENUM(BlueprintType)
enum class ECommonQTETestState : uint8
{
	NotStarted,
	Running,
	Passed,
	Failed,
	Cancelled,
};

UENUM(BlueprintType)
enum class ECommonQTETestStep : uint8
{
	None,
	CreateQTE,
	VerifyInitialState,
	WaitingForInput,
	SubmitInput,
	VerifyPredictionOnly,
	VerifyAuthorityApplied,
	VerifyPerformerReconciled,
	VerifyObserverApplied,
	Finished,
};

UENUM(BlueprintType)
enum class ECommonQTETestCellSource : uint8
{
	GenerateConfig,
	FixedCellContents,
};

UENUM(BlueprintType)
enum class ECommonQTETestInputBehavior : uint8
{
	PerformerCorrect,
	PerformerWrong,
	PerformerCheatWrongContentButLocalCorrect,
};

UENUM(BlueprintType)
enum class ECommonQTETestNetworkTopology : uint8
{
	None,
	DedicatedServer,
	ListenServer,
};

UENUM(BlueprintType)
enum class ECommonQTETestListenServerLayout : uint8
{
	HostAsPerformer,
	RemoteClientPerformer,
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestNetworkSimulationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bEnableNetworkSimulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation"))
	ECommonQTETestNetworkTopology Topology = ECommonQTETestNetworkTopology::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation"))
	ECommonQTETestListenServerLayout ListenServerLayout = ECommonQTETestListenServerLayout::HostAsPerformer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0"))
	float PerformerToAuthorityDelaySeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0"))
	float AuthorityToPerformerDelaySeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0"))
	float AuthorityToObserverDelaySeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation", ClampMin = "0.0"))
	float VerificationSettleSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bEnableNetworkSimulation"))
	bool bVerifyEndpointIsolation = true;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestVisualizationConfig
{
	GENERATED_BODY()

	FCommonQTETestVisualizationConfig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bPrintScreenLog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bLogToOutputLog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bCreateResultWidget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (EditCondition = "bCreateResultWidget", AllowAbstract = "false"))
	TSubclassOf<UCommonQTEFlowTestWidget> ResultWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bCreateSlatePanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bShowSlateControlPanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bShowSlateVisualizer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bShowAuthorityView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bShowPerformerView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bShowObserverView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float SlateVisualizerScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bPauseBetweenSteps = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bAllowManualInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bKeepPanelAfterFinish = true;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestConfig
{
	GENERATED_BODY()

	FCommonQTETestConfig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestCellSource CellSource = ECommonQTETestCellSource::GenerateConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	TArray<int32> FixedCellContents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTECellGenerateConfig GenerateConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 WrongCellContent = 99;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	float StepTimeoutSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTERule Rule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEPresentationDrainConfig PresentationDrainConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bCreateObserver = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bAutoStopWhenFinished = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bAutoFinishWhenTerminal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bUseTestObserverForceRouting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTETestNetworkSimulationConfig NetworkSimulationConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTETestVisualizationConfig VisualizationConfig;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestStepResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestStep Step = ECommonQTETestStep::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestInputBehavior InputBehavior = ECommonQTETestInputBehavior::PerformerCorrect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bPassed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEHandle Handle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTEState AuthorityState = ECommonQTEState::Fail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTEState PerformerPredictedState = ECommonQTEState::Fail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LocalCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 OutboundCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PredictionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 RollbackCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 ObserverDeltaCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PresentationMessageCount = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestState State = ECommonQTETestState::NotStarted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestStep CurrentStep = ECommonQTETestStep::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestInputBehavior LastInputBehavior = ECommonQTETestInputBehavior::PerformerCorrect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bPassed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FString Summary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEHandle CurrentHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	TArray<FCommonQTEHandle> StartedHandles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	TArray<FCommonQTETestStepResult> StepResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 CurrentCellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 CurrentExpectedCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastPredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastLocalCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastOutboundCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PredictionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 RollbackCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 ObserverDeltaCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PresentationMessageCount = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTEPresentationVisualState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bHasState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEHandle Handle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTEPresentationSource LastSource = ECommonQTEPresentationSource::Observer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTEPresentationPhase LastPhase = ECommonQTEPresentationPhase::StateDelta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTEState QTEState = ECommonQTEState::OnGoing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	TArray<int32> CellContents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	TArray<ECommonQTECellState> CellStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastChangedCellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastSequenceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastPredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 MessageCount = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestEndpointState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bHasSnapshot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot Snapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastPredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastSequenceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PendingPacketCount = 0;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTEFlowTestVisualState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEHandle Handle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestStep Step = ECommonQTETestStep::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestInputBehavior LastInputBehavior = ECommonQTETestInputBehavior::PerformerCorrect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestNetworkTopology NetworkTopology = ECommonQTETestNetworkTopology::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	ECommonQTETestListenServerLayout ListenServerLayout = ECommonQTETestListenServerLayout::HostAsPerformer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bInputInProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 CurrentCellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 CurrentExpectedCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastPredictionId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastLocalCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LastOutboundCellContent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bHasAuthoritySnapshot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot AuthoritySnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bHasPerformerSnapshot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot PerformerSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEPresentationVisualState PerformerPresentationState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEPresentationVisualState ObserverPresentationState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTETestEndpointState AuthorityEndpointState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTETestEndpointState PerformerEndpointState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTETestEndpointState ObserverEndpointState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bPerformerPresentationMatchesPerformer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bObserverPresentationMatchesAuthority = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bObserverCreated = false;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestPerformerTrace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bReceivedInitialState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PredictionApplyCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 RollbackApplyCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 LocalPredictionDrainCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 RollbackDrainCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 PredictionPresentationRouteCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 RollbackPresentationRouteCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastInitialStateSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastStateBeforePrediction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastStateAfterPrediction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastStateBeforeRollback;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastStateAfterRollback;
};

USTRUCT(BlueprintType)
struct COMMONQTE_TEST_API FCommonQTETestObserverTrace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	bool bReceivedInitialState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 StateDeltaCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 InitialPresentationBuildCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	int32 DeltaPresentationBuildCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEStateSnapshot LastInitialStateSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommonQTE|Test")
	FCommonQTEObserverStateDelta LastStateDelta;
};
