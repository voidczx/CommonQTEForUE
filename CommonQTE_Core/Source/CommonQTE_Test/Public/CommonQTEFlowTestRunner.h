#pragma once

#include "CoreMinimal.h"
#include "CommonQTEHandle.h"
#include "CommonQTETestType.h"
#include "UObject/Object.h"

#include "CommonQTEFlowTestRunner.generated.h"

class AActor;
class ACommonQTEPerformerActor;
class ACommonQTETestObserverActor;
class ACommonQTETestPerformerActor;
class FCommonQTECellGeneratePolicy;
class UCommonQTEFlowTestWidget;
class UGameViewportClient;
class UCommonQTEManagerComponent;
class SWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonQTEFlowTestFinished, const FCommonQTETestReport&, Report);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonQTEFlowTestStepFinished, const FCommonQTETestStepResult&, StepResult);

struct FCommonQTEDelayedPresentationMessageBatch
{
	TArray<FCommonQTEPresentationMessage> Messages;
	double DeliveryTimeSeconds = 0.0;
};

struct FCommonQTETestInputRecord
{
	ECommonQTETestInputBehavior Behavior = ECommonQTETestInputBehavior::PerformerCorrect;
	int32 PredictionId = 0;
	int32 CellIndex = INDEX_NONE;
	int32 ExpectedCellContent = 0;
	int32 LocalCellContent = 0;
	int32 OutboundCellContent = 0;
	int32 PreInputPredictionApplyCount = 0;
	int32 PreInputRollbackApplyCount = 0;
	int32 ExpectedObserverSequenceId = 0;
	bool bLocalExpectedMatched = false;
	bool bAuthorityExpectedMatched = false;
};

UCLASS(BlueprintType)
class COMMONQTE_TEST_API UCommonQTEFlowTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE|Test")
	FCommonQTEFlowTestFinished OnTestFinished;

	UPROPERTY(BlueprintAssignable, Category = "CommonQTE|Test")
	FCommonQTEFlowTestStepFinished OnTestStepFinished;

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test", meta = (WorldContext = "WorldContextObject"))
	void StartTest(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig);

	void StartTestWithGeneratePolicy(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig, TSharedPtr<FCommonQTECellGeneratePolicy> InCellGeneratePolicy);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void ContinueTest();

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	bool SubmitInput(ECommonQTETestInputBehavior Behavior);

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void StopTest();

	UFUNCTION(BlueprintCallable, Category = "CommonQTE|Test")
	void CancelTest();

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FCommonQTETestReport GetReport() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FString GetSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "CommonQTE|Test")
	FCommonQTEFlowTestVisualState GetVisualState() const;

private:
	UFUNCTION()
	void HandlePresentationMessagesReady(const TArray<FCommonQTEPresentationMessage>& Messages);

	UFUNCTION()
	void HandleLocalPredictionMessagesReady(const TArray<FCommonQTEPerformerPredictionMessage>& Messages);

	UFUNCTION()
	void HandlePerformerRollBackMessagesReady(const TArray<FCommonQTEPerformerRollBackMessage>& Messages);

	UFUNCTION()
	void HandlePerformerVerificationApplied(ACommonQTEPerformerActor* Performer, const FCommonQTEPerformerPredictionMessage& Message, const FCommonQTEApplyInputResult& ApplyResult);

	void InitializeTest(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig, TSharedPtr<FCommonQTECellGeneratePolicy> InCellGeneratePolicy);
	void ResetRuntimeState();
	void SetupVisualization();
	void TeardownVisualization(bool bForceRemove);
	void BindWorldLifecycle(UWorld* World);
	void UnbindWorldLifecycle();
	void HandleWorldBeginTearDown(UWorld* World);
	void CleanupForWorldTearDown();
	bool ShouldKeepRunnerForVisualization() const;
	void CreateQTE();
	void EnterWaitingForInput();
	void CompleteInputCycle();
	void HandleStepTimeout();
	void ExecuteStep(ECommonQTETestStep InStep);
	void BeginWaitingForStep(ECommonQTETestStep InStep);
	void TriggerStep(ECommonQTETestStep InStep);
	void TriggerWaitingStep(ECommonQTETestStep InStep);
	void TriggerObservedWaitingStep(ECommonQTETestStep InStep);
	void VerifyInitialState();
	void VerifyPredictionOnly();
	void VerifyAuthorityApplied();
	void VerifyPerformerReconciled();
	void VerifyObserverApplied();
	void FinishTest(bool bPassed, const FString& Summary);
	void FailTest(const FString& Message);
	void AddStepResult(ECommonQTETestStep Step, bool bPassed, const FString& Message);
	void BroadcastReport();
	void PrintVisualMessage(const FString& Message, bool bError) const;
	void RefreshVisualState();
	void ResetVisualStateForCurrentQTE();
	void ResetStateSnapshotCaches();
	void CacheAuthoritySnapshot(const FCommonQTEStateSnapshot& Snapshot);
	void CachePerformerSnapshot(const FCommonQTEStateSnapshot& Snapshot);
	void CapturePreInputEndpointState();
	void ResetPreInputEndpointState();
	void ApplyPresentationMessageToState(const FCommonQTEPresentationMessage& Message, FCommonQTEPresentationVisualState& VisualPresentationState);
	void ApplyObserverPresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages);
	bool IsObserverPresentationReadyForCurrentInput();
	void QueueObserverPresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages);
	void ScheduleNetworkSimulationDrain();
	void DrainNetworkSimulationQueues();
	void CleanupCurrentQTE();
	void BindManagerPresentationEvent();
	void UnbindManagerPresentationEvent();
	void BindPerformerRollBackEvent();
	void UnbindPerformerRollBackEvent();
	void ResetInputEventWaits();
	bool DoesPredictionEventMatchCurrentInput(const FCommonQTEPerformerPredictionMessage& Message) const;
	bool DoesRollBackEventMatchCurrentInput(const FCommonQTEPerformerRollBackMessage& Message) const;
	void RecordCurrentPredictionId(int32 PredictionId);
	bool ResolveRuntimeActors();
	bool HasValidWorld() const;
	bool GetAuthoritySnapshot(FCommonQTEStateSnapshot& OutSnapshot);
	bool GetPerformerSnapshot(FCommonQTEStateSnapshot& OutSnapshot);
	bool BuildObserverSnapshot(FCommonQTEStateSnapshot& OutSnapshot) const;
	bool AreStateSnapshotsEquivalent(const FCommonQTEStateSnapshot& Left, const FCommonQTEStateSnapshot& Right) const;
	bool DoesPresentationStateMatchSnapshot(const FCommonQTEPresentationVisualState& PresentationState, const FCommonQTEStateSnapshot& Snapshot) const;
	bool IsNetworkSimulationEnabled() const;
	bool IsListenServerHostPerformer() const;
	bool IsTerminalState(ECommonQTEState State) const;
	float GetPerformerToAuthorityDelayForTest() const;
	float GetAuthorityToPerformerDelayForTest() const;
	float GetAuthorityToObserverDelayForTest() const;
	float GetVerificationSettleDelay() const;
	int32 GetPendingObserverPresentationMessageCount() const;
	int32 GetCurrentAuthorityCellIndex() const;
	int32 GetExpectedCorrectInput() const;
	int32 GetResolvedWrongInput(int32 ExpectedInput) const;
	ECommonQTEState GetAuthorityState();
	ECommonQTEState GetPerformerPredictedState();
	FString GetStepName(ECommonQTETestStep Step) const;
	FString GetInputBehaviorName(ECommonQTETestInputBehavior Behavior) const;

	UPROPERTY()
	FCommonQTETestConfig Config;

	UPROPERTY()
	FCommonQTETestReport Report;

	UPROPERTY()
	FCommonQTEFlowTestVisualState VisualState;

	UPROPERTY()
	TObjectPtr<UCommonQTEFlowTestWidget> ResultWidget;

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AActor> PerformerOwner;
	TWeakObjectPtr<UCommonQTEManagerComponent> Manager;
	TWeakObjectPtr<ACommonQTETestPerformerActor> CurrentPerformer;
	TWeakObjectPtr<ACommonQTETestPerformerActor> BoundRollBackEventPerformer;
	TWeakObjectPtr<ACommonQTETestObserverActor> CurrentObserver;
	TWeakObjectPtr<UGameViewportClient> SlateViewport;
	TSharedPtr<SWidget> SlatePanel;
	TSharedPtr<FCommonQTECellGeneratePolicy> CustomCellGeneratePolicy;
	FTimerHandle StepTimeoutTimerHandle;
	FTimerHandle NetworkSimulationTimerHandle;
	FDelegateHandle WorldBeginTearDownHandle;
	FCommonQTEHandle CurrentHandle;
	ECommonQTETestStep PendingStep = ECommonQTETestStep::None;
	TArray<int32> CurrentCellContents;
	TArray<FCommonQTEDelayedPresentationMessageBatch> DelayedObserverPresentationBatches;
	FCommonQTEStateSnapshot CachedAuthoritySnapshot;
	FCommonQTEStateSnapshot CachedPerformerSnapshot;
	FCommonQTEStateSnapshot PreInputAuthoritySnapshot;
	FCommonQTEStateSnapshot PreInputPerformerSnapshot;
	FCommonQTETestInputRecord LastInputRecord;
	bool bWaitingForManualContinue = false;
	bool bPresentationEventBound = false;
	bool bHasCachedAuthoritySnapshot = false;
	bool bHasCachedPerformerSnapshot = false;
	bool bHasPreInputAuthoritySnapshot = false;
	bool bHasPreInputPerformerSnapshot = false;
	bool bWaitingForPredictionApply = false;
	bool bWaitingForAuthorityApply = false;
	bool bWaitingForPerformerReconcile = false;
	bool bWaitingForObserverPresentationApply = false;
	bool bPredictionApplyObserved = false;
	bool bAuthorityApplyObserved = false;
	bool bPerformerReconcileObserved = false;
	bool bInputInProgress = false;
	bool bAddedToRootForAsyncRun = false;
};
