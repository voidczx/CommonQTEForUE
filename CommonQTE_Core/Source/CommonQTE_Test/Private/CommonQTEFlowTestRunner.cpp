#include "CommonQTEFlowTestRunner.h"

#include "CommonQTEActorClassConfig.h"
#include "CommonQTECellGeneratePolicy.h"
#include "CommonQTEFlowTestWidget.h"
#include "CommonQTELog.h"
#include "CommonQTEManagerComponent.h"
#include "CommonQTEObserverActor.h"
#include "CommonQTEPerformerActor.h"
#include "CommonQTELibrary.h"
#include "CommonQTEStateRuntime.h"
#include "CommonQTETestObserverActor.h"
#include "CommonQTETestPerformerActor.h"
#include "CommonQTETestLog.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SCommonQTEFlowTestPanel.h"
#include "TimerManager.h"

UWorld* UCommonQTEFlowTestRunner::GetWorld() const
{
	return CachedWorld.Get();
}

void UCommonQTEFlowTestRunner::BeginDestroy()
{
	CancelTest();
	Super::BeginDestroy();
}

void UCommonQTEFlowTestRunner::StartTest(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig)
{
	InitializeTest(WorldContextObject, InPerformerOwner, InConfig, TSharedPtr<FCommonQTECellGeneratePolicy>());
}

void UCommonQTEFlowTestRunner::StartTestWithGeneratePolicy(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig, TSharedPtr<FCommonQTECellGeneratePolicy> InCellGeneratePolicy)
{
	InitializeTest(WorldContextObject, InPerformerOwner, InConfig, InCellGeneratePolicy);
}

void UCommonQTEFlowTestRunner::InitializeTest(UObject* WorldContextObject, AActor* InPerformerOwner, FCommonQTETestConfig InConfig, TSharedPtr<FCommonQTECellGeneratePolicy> InCellGeneratePolicy)
{
	ResetRuntimeState();

	UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World == nullptr)
	{
		FailTest(TEXT("World context is invalid."));
		return;
	}

	CachedWorld = World;
	BindWorldLifecycle(World);
	PerformerOwner = InPerformerOwner;
	Config = InConfig;
	CustomCellGeneratePolicy = InCellGeneratePolicy;
	Config.StepTimeoutSeconds = FMath::Max(0.0f, Config.StepTimeoutSeconds);
	Config.PresentationDrainConfig.MaxMessagesPerDrain = FMath::Max(0, Config.PresentationDrainConfig.MaxMessagesPerDrain);
	Config.PresentationDrainConfig.DrainInterval = FMath::Max(0.0f, Config.PresentationDrainConfig.DrainInterval);
	Config.VisualizationConfig.SlateVisualizerScale = FMath::Clamp(Config.VisualizationConfig.SlateVisualizerScale, 0.5f, 2.0f);
	Config.NetworkSimulationConfig.PerformerToAuthorityDelaySeconds = FMath::Max(0.0f, Config.NetworkSimulationConfig.PerformerToAuthorityDelaySeconds);
	Config.NetworkSimulationConfig.AuthorityToPerformerDelaySeconds = FMath::Max(0.0f, Config.NetworkSimulationConfig.AuthorityToPerformerDelaySeconds);
	Config.NetworkSimulationConfig.AuthorityToObserverDelaySeconds = FMath::Max(0.0f, Config.NetworkSimulationConfig.AuthorityToObserverDelaySeconds);
	Config.NetworkSimulationConfig.VerificationSettleSeconds = FMath::Max(0.0f, Config.NetworkSimulationConfig.VerificationSettleSeconds);

	Report.State = ECommonQTETestState::Running;
	Report.Summary = TEXT("CommonQTE test is creating QTE.");

	if (!bAddedToRootForAsyncRun)
	{
		AddToRoot();
		bAddedToRootForAsyncRun = true;
	}

	Manager = UCommonQTELibrary::GetCommonQTEManager(WorldContextObject, true);
	if (!Manager.IsValid())
	{
		FailTest(TEXT("CommonQTE manager is missing."));
		return;
	}

	Manager->SetPresentationDrainConfig(Config.PresentationDrainConfig);
	BindManagerPresentationEvent();
	SetupVisualization();
	BroadcastReport();
	PrintVisualMessage(Report.Summary, false);
	CreateQTE();
}

void UCommonQTEFlowTestRunner::ContinueTest()
{
	if (Report.State != ECommonQTETestState::Running || !bWaitingForManualContinue)
	{
		return;
	}

	bWaitingForManualContinue = false;
	const ECommonQTETestStep StepToExecute = PendingStep;
	if (StepToExecute == ECommonQTETestStep::None)
	{
		return;
	}
	PendingStep = ECommonQTETestStep::None;
	ExecuteStep(StepToExecute);
}

bool UCommonQTEFlowTestRunner::SubmitInput(ECommonQTETestInputBehavior Behavior)
{
	if (Report.State != ECommonQTETestState::Running)
	{
		return false;
	}

	if (bInputInProgress || PendingStep != ECommonQTETestStep::None || Report.CurrentStep != ECommonQTETestStep::WaitingForInput)
	{
		PrintVisualMessage(TEXT("Input rejected because the previous input is still being verified."), true);
		return false;
	}

	if (!CurrentPerformer.IsValid())
	{
		FailTest(TEXT("Cannot submit input because performer is missing."));
		return false;
	}

	FCommonQTEStateSnapshot AuthoritySnapshot;
	if (!GetAuthoritySnapshot(AuthoritySnapshot))
	{
		FailTest(TEXT("Cannot submit input because authority snapshot is missing."));
		return false;
	}

	if (IsTerminalState(AuthoritySnapshot.QTEState))
	{
		PrintVisualMessage(TEXT("Input rejected because QTE is already terminal."), true);
		if (Config.bAutoFinishWhenTerminal)
		{
			FinishTest(true, TEXT("CommonQTE test reached terminal state."));
		}
		return false;
	}

	const int32 CellIndex = AuthoritySnapshot.DriverState.CellIndex;
	if (!AuthoritySnapshot.CellContents.IsValidIndex(CellIndex))
	{
		FailTest(FString::Printf(TEXT("Cannot submit input because current cell index is invalid. CellIndex=%d CellCount=%d"), CellIndex, AuthoritySnapshot.CellContents.Num()));
		return false;
	}

	const int32 ExpectedContent = AuthoritySnapshot.CellContents[CellIndex];
	const int32 WrongContent = GetResolvedWrongInput(ExpectedContent);
	int32 LocalContent = ExpectedContent;
	int32 OutboundContent = ExpectedContent;
	switch (Behavior)
	{
	case ECommonQTETestInputBehavior::PerformerWrong:
		LocalContent = WrongContent;
		OutboundContent = WrongContent;
		break;
	case ECommonQTETestInputBehavior::PerformerCheatWrongContentButLocalCorrect:
		LocalContent = ExpectedContent;
		OutboundContent = WrongContent;
		break;
	case ECommonQTETestInputBehavior::PerformerCorrect:
	default:
		break;
	}

	CapturePreInputEndpointState();
	const FCommonQTETestPerformerTrace PreInputTrace = CurrentPerformer->GetTestTrace();
	ResetInputEventWaits();
	LastInputRecord = FCommonQTETestInputRecord();
	LastInputRecord.Behavior = Behavior;
	LastInputRecord.CellIndex = CellIndex;
	LastInputRecord.ExpectedCellContent = ExpectedContent;
	LastInputRecord.LocalCellContent = LocalContent;
	LastInputRecord.OutboundCellContent = OutboundContent;
	LastInputRecord.PreInputPredictionApplyCount = PreInputTrace.PredictionApplyCount;
	LastInputRecord.PreInputRollbackApplyCount = PreInputTrace.RollbackApplyCount;
	LastInputRecord.bLocalExpectedMatched = LocalContent == ExpectedContent;
	LastInputRecord.bAuthorityExpectedMatched = OutboundContent == ExpectedContent;
	bInputInProgress = true;

	FCommonQTEPerformerPredictionMessage PredictionMessage = CurrentPerformer->SubmitPredictionForTest(LocalContent, OutboundContent);
	if (PredictionMessage.PredictionId <= 0)
	{
		FailTest(TEXT("Cannot submit input because prediction message is invalid."));
		return false;
	}

	RecordCurrentPredictionId(PredictionMessage.PredictionId);
	bWaitingForPredictionApply = true;

	Report.LastInputBehavior = Behavior;
	Report.LastPredictionId = LastInputRecord.PredictionId;
	Report.LastLocalCellContent = LocalContent;
	Report.LastOutboundCellContent = OutboundContent;

	AddStepResult(
		ECommonQTETestStep::SubmitInput,
		true,
		FString::Printf(
			TEXT("Submitted input. Behavior=%s CellIndex=%d PredictionId=%d Expected=%d Local=%d Outbound=%d"),
			*GetInputBehaviorName(Behavior),
			CellIndex,
			LastInputRecord.PredictionId,
			ExpectedContent,
			LocalContent,
			OutboundContent));
	BeginWaitingForStep(ECommonQTETestStep::VerifyPredictionOnly);
	return true;
}

void UCommonQTEFlowTestRunner::StopTest()
{
	CancelTest();
}

void UCommonQTEFlowTestRunner::CancelTest()
{
	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
		CachedWorld->GetTimerManager().ClearTimer(NetworkSimulationTimerHandle);
	}

	if (Report.State == ECommonQTETestState::NotStarted || Report.State == ECommonQTETestState::Passed || Report.State == ECommonQTETestState::Failed || Report.State == ECommonQTETestState::Cancelled)
	{
		UnbindWorldLifecycle();
		UnbindManagerPresentationEvent();
		CleanupCurrentQTE();
		TeardownVisualization(true);
		CachedWorld.Reset();
		if (bAddedToRootForAsyncRun)
		{
			RemoveFromRoot();
			bAddedToRootForAsyncRun = false;
		}
		return;
	}

	Report.State = ECommonQTETestState::Cancelled;
	Report.bPassed = false;
	Report.Summary = TEXT("CommonQTE test was cancelled.");
	Report.CurrentStep = ECommonQTETestStep::Finished;
	PendingStep = ECommonQTETestStep::None;
	bInputInProgress = false;
	ResetInputEventWaits();
	UnbindWorldLifecycle();
	UnbindManagerPresentationEvent();
	DelayedObserverPresentationBatches.Reset();
	CleanupCurrentQTE();
	TeardownVisualization(true);
	CachedWorld.Reset();
	BroadcastReport();
	OnTestFinished.Broadcast(Report);

	if (bAddedToRootForAsyncRun)
	{
		RemoveFromRoot();
		bAddedToRootForAsyncRun = false;
	}
}

FCommonQTETestReport UCommonQTEFlowTestRunner::GetReport() const
{
	return Report;
}

FString UCommonQTEFlowTestRunner::GetSummaryText() const
{
	FString LatestStepMessage;
	if (!Report.StepResults.IsEmpty())
	{
		LatestStepMessage = Report.StepResults.Last().Message;
	}

	return FString::Printf(
		TEXT("State: %s\nStep: %s\nSummary: %s\nHandle: %d\nCellIndex: %d\nExpected: %d\nLast Input: %s\nLast Prediction: %d\nLocal Content: %d\nOutbound Content: %d\nPredictions: %d\nRollbacks: %d\nObserver Deltas: %d\nPresentation Messages: %d\nLatest: %s"),
		*StaticEnum<ECommonQTETestState>()->GetNameStringByValue(static_cast<int64>(Report.State)),
		*GetStepName(Report.CurrentStep),
		*Report.Summary,
		Report.CurrentHandle.GetValue(),
		Report.CurrentCellIndex,
		Report.CurrentExpectedCellContent,
		*GetInputBehaviorName(Report.LastInputBehavior),
		Report.LastPredictionId,
		Report.LastLocalCellContent,
		Report.LastOutboundCellContent,
		Report.PredictionCount,
		Report.RollbackCount,
		Report.ObserverDeltaCount,
		Report.PresentationMessageCount,
		*LatestStepMessage);
}

FCommonQTEFlowTestVisualState UCommonQTEFlowTestRunner::GetVisualState() const
{
	return VisualState;
}

void UCommonQTEFlowTestRunner::HandlePresentationMessagesReady(const TArray<FCommonQTEPresentationMessage>& Messages)
{
	Report.PresentationMessageCount += Messages.Num();
	TArray<FCommonQTEPresentationMessage> ObserverMessages;
	for (const FCommonQTEPresentationMessage& Message : Messages)
	{
		if (CurrentHandle.IsValid() && Message.WholeHandle != CurrentHandle)
		{
			continue;
		}

		switch (Message.Source)
		{
		case ECommonQTEPresentationSource::Observer:
			ObserverMessages.Add(Message);
			break;
		case ECommonQTEPresentationSource::Performer:
			ApplyPresentationMessageToState(Message, VisualState.PerformerPresentationState);
			break;
		default:
			break;
		}
	}

	if (!ObserverMessages.IsEmpty())
	{
		if (IsNetworkSimulationEnabled() && GetAuthorityToObserverDelayForTest() > 0.0f)
		{
			QueueObserverPresentationMessages(ObserverMessages);
		}
		else
		{
			ApplyObserverPresentationMessages(ObserverMessages);
		}
	}
	BroadcastReport();
}

void UCommonQTEFlowTestRunner::HandleLocalPredictionMessagesReady(const TArray<FCommonQTEPerformerPredictionMessage>& Messages)
{
	for (const FCommonQTEPerformerPredictionMessage& Message : Messages)
	{
		if (DoesPredictionEventMatchCurrentInput(Message))
		{
			RecordCurrentPredictionId(Message.PredictionId);
			bPredictionApplyObserved = true;
			if (bWaitingForPredictionApply)
			{
				bWaitingForPredictionApply = false;
				TriggerWaitingStep(ECommonQTETestStep::VerifyPredictionOnly);
			}
			return;
		}
	}
}

void UCommonQTEFlowTestRunner::HandlePerformerRollBackMessagesReady(const TArray<FCommonQTEPerformerRollBackMessage>& Messages)
{
	for (const FCommonQTEPerformerRollBackMessage& Message : Messages)
	{
		if (CurrentHandle.IsValid() && Message.WholeHandle != CurrentHandle)
		{
			continue;
		}

		if (DoesRollBackEventMatchCurrentInput(Message))
		{
			FCommonQTEStateSnapshot RollBackBaseSnapshot = bHasCachedAuthoritySnapshot ? CachedAuthoritySnapshot : FCommonQTEStateSnapshot();
			if (!RollBackBaseSnapshot.WholeHandle.IsValid() && CurrentPerformer.IsValid())
			{
				RollBackBaseSnapshot = CurrentPerformer->GetInitialStateSnapshot();
			}
			const FCommonQTEStateSnapshot AuthoritativeStateSnapshot = FCommonQTEStateRuntime::ExpandCompactStateSnapshot(Message.AuthoritativeStateSnapshot, RollBackBaseSnapshot);
			CacheAuthoritySnapshot(AuthoritativeStateSnapshot);
			CachePerformerSnapshot(AuthoritativeStateSnapshot);
			bPerformerReconcileObserved = true;
			if (bWaitingForPerformerReconcile)
			{
				bWaitingForPerformerReconcile = false;
				TriggerWaitingStep(ECommonQTETestStep::VerifyPerformerReconciled);
			}
			return;
		}
	}
	BroadcastReport();
}

void UCommonQTEFlowTestRunner::HandlePerformerVerificationApplied(ACommonQTEPerformerActor* Performer, const FCommonQTEPerformerPredictionMessage& Message, const FCommonQTEApplyInputResult& ApplyResult)
{
	if (Performer != CurrentPerformer.Get() || !DoesPredictionEventMatchCurrentInput(Message))
	{
		return;
	}

	RecordCurrentPredictionId(Message.PredictionId);
	CacheAuthoritySnapshot(ApplyResult.StateAfterInput);
	if (CurrentObserver.IsValid())
	{
		LastInputRecord.ExpectedObserverSequenceId = CurrentObserver->GetTestTrace().LastStateDelta.SequenceId;
	}

	bAuthorityApplyObserved = true;
	if (bWaitingForAuthorityApply)
	{
		bWaitingForAuthorityApply = false;
		TriggerWaitingStep(ECommonQTETestStep::VerifyAuthorityApplied);
	}
}

void UCommonQTEFlowTestRunner::ResetRuntimeState()
{
	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
		CachedWorld->GetTimerManager().ClearTimer(NetworkSimulationTimerHandle);
	}

	UnbindWorldLifecycle();
	UnbindManagerPresentationEvent();
	CleanupCurrentQTE();
	TeardownVisualization(true);

	Config = FCommonQTETestConfig();
	Report = FCommonQTETestReport();
	VisualState = FCommonQTEFlowTestVisualState();
	ResetStateSnapshotCaches();
	ResultWidget = nullptr;
	PerformerOwner.Reset();
	CachedWorld.Reset();
	Manager.Reset();
	CurrentPerformer.Reset();
	CurrentObserver.Reset();
	CustomCellGeneratePolicy.Reset();
	CurrentHandle.Reset();
	PendingStep = ECommonQTETestStep::None;
	CurrentCellContents.Reset();
	DelayedObserverPresentationBatches.Reset();
	ResetPreInputEndpointState();
	LastInputRecord = FCommonQTETestInputRecord();
	bWaitingForManualContinue = false;
	bInputInProgress = false;
	ResetInputEventWaits();

	if (bAddedToRootForAsyncRun)
	{
		RemoveFromRoot();
		bAddedToRootForAsyncRun = false;
	}
}

void UCommonQTEFlowTestRunner::SetupVisualization()
{
	if (Config.VisualizationConfig.bCreateResultWidget)
	{
		UWorld* World = CachedWorld.Get();
		APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		if (PlayerController != nullptr && Config.VisualizationConfig.ResultWidgetClass != nullptr)
		{
			ResultWidget = CreateWidget<UCommonQTEFlowTestWidget>(PlayerController, Config.VisualizationConfig.ResultWidgetClass);
			if (ResultWidget != nullptr)
			{
				ResultWidget->InitializeWithRunner(this);
				ResultWidget->AddToViewport();
				ResultWidget->OnTestReportUpdated(Report);
			}
		}
		else
		{
			PrintVisualMessage(TEXT("Result widget was requested but player controller or widget class is missing."), true);
		}
	}

	UGameViewportClient* GameViewport = GEngine != nullptr ? GEngine->GameViewport : nullptr;
	if (Config.VisualizationConfig.bCreateSlatePanel && GameViewport != nullptr)
	{
		SlatePanel = SNew(SCommonQTEFlowTestPanel)
			.Runner(this)
			.bAllowManualInput(Config.VisualizationConfig.bAllowManualInput)
			.bShowControlPanel(Config.VisualizationConfig.bShowSlateControlPanel)
			.bShowVisualizer(Config.VisualizationConfig.bShowSlateVisualizer)
			.bShowAuthorityView(Config.VisualizationConfig.bShowAuthorityView)
			.bShowPerformerView(Config.VisualizationConfig.bShowPerformerView)
			.bShowObserverView(Config.VisualizationConfig.bShowObserverView)
			.VisualizerScale(Config.VisualizationConfig.SlateVisualizerScale);
		SlateViewport = GameViewport;
		GameViewport->AddViewportWidgetContent(SlatePanel.ToSharedRef());
	}
}

void UCommonQTEFlowTestRunner::TeardownVisualization(bool bForceRemove)
{
	const bool bKeepVisualization = Config.VisualizationConfig.bKeepPanelAfterFinish && !bForceRemove;
	if (!bKeepVisualization && ResultWidget != nullptr)
	{
		ResultWidget->InitializeWithRunner(nullptr);
		ResultWidget->RemoveFromParent();
		ResultWidget = nullptr;
	}

	if (!bKeepVisualization && SlatePanel.IsValid())
	{
		if (UGameViewportClient* GameViewport = SlateViewport.Get())
		{
			GameViewport->RemoveViewportWidgetContent(SlatePanel.ToSharedRef());
		}
		else if (GEngine != nullptr && GEngine->GameViewport != nullptr)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(SlatePanel.ToSharedRef());
		}
		SlatePanel.Reset();
		SlateViewport.Reset();
	}
}

void UCommonQTEFlowTestRunner::BindWorldLifecycle(UWorld* World)
{
	UnbindWorldLifecycle();
	if (World != nullptr)
	{
		WorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UCommonQTEFlowTestRunner::HandleWorldBeginTearDown);
	}
}

void UCommonQTEFlowTestRunner::UnbindWorldLifecycle()
{
	if (WorldBeginTearDownHandle.IsValid())
	{
		FWorldDelegates::OnWorldBeginTearDown.Remove(WorldBeginTearDownHandle);
		WorldBeginTearDownHandle.Reset();
	}
}

void UCommonQTEFlowTestRunner::HandleWorldBeginTearDown(UWorld* World)
{
	if (World == nullptr || World != CachedWorld.Get())
	{
		return;
	}

	CleanupForWorldTearDown();
}

void UCommonQTEFlowTestRunner::CleanupForWorldTearDown()
{
	if (UWorld* World = CachedWorld.Get())
	{
		World->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
		World->GetTimerManager().ClearTimer(NetworkSimulationTimerHandle);
	}

	if (Report.State == ECommonQTETestState::Running || Report.State == ECommonQTETestState::NotStarted)
	{
		Report.State = ECommonQTETestState::Cancelled;
		Report.bPassed = false;
		Report.Summary = TEXT("CommonQTE test was cancelled because its world is tearing down.");
		Report.CurrentStep = ECommonQTETestStep::Finished;
	}

	PendingStep = ECommonQTETestStep::None;
	bInputInProgress = false;
	ResetInputEventWaits();
	UnbindManagerPresentationEvent();
	UnbindPerformerRollBackEvent();
	DelayedObserverPresentationBatches.Reset();
	TeardownVisualization(true);
	ResultWidget = nullptr;
	SlatePanel.Reset();
	SlateViewport.Reset();
	PerformerOwner.Reset();
	Manager.Reset();
	CurrentPerformer.Reset();
	BoundRollBackEventPerformer.Reset();
	CurrentObserver.Reset();
	CurrentHandle.Reset();
	Report.CurrentHandle.Reset();
	CachedWorld.Reset();
	UnbindWorldLifecycle();

	if (bAddedToRootForAsyncRun)
	{
		RemoveFromRoot();
		bAddedToRootForAsyncRun = false;
	}
}

bool UCommonQTEFlowTestRunner::ShouldKeepRunnerForVisualization() const
{
	return Config.VisualizationConfig.bKeepPanelAfterFinish && (ResultWidget != nullptr || SlatePanel.IsValid());
}

void UCommonQTEFlowTestRunner::CreateQTE()
{
	if (!Manager.IsValid())
	{
		FailTest(TEXT("Cannot create QTE because manager is missing."));
		return;
	}

	CleanupCurrentQTE();
	ResetVisualStateForCurrentQTE();

	FCommonQTEActorClassConfig ActorClassConfig;
	ActorClassConfig.PerformerActorClass = ACommonQTETestPerformerActor::StaticClass();
	ActorClassConfig.ObserverActorClass = ACommonQTETestObserverActor::StaticClass();

	if (CustomCellGeneratePolicy.IsValid())
	{
		CurrentHandle = Manager->CreateEntityWithPolicy(Config.Rule, CustomCellGeneratePolicy, ActorClassConfig);
	}
	else if (Config.CellSource == ECommonQTETestCellSource::FixedCellContents)
	{
		CurrentHandle = Manager->CreateEntityWithFixedCells(Config.Rule, Config.FixedCellContents, ActorClassConfig);
	}
	else
	{
		CurrentHandle = Manager->CreateEntity(Config.Rule, Config.GenerateConfig, ActorClassConfig);
	}

	Report.CurrentHandle = CurrentHandle;
	if (CurrentHandle.IsValid())
	{
		Report.StartedHandles.Add(CurrentHandle);
		Manager->AddPerformer(CurrentHandle, PerformerOwner.Get());
		if (Config.bCreateObserver)
		{
			Manager->CreateObserver(CurrentHandle);
		}
	}

	const bool bResolvedActors = ResolveRuntimeActors();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	if (bAuthoritySnapshotValid)
	{
		CurrentCellContents = AuthoritySnapshot.CellContents;
	}

	AddStepResult(
		ECommonQTETestStep::CreateQTE,
		CurrentHandle.IsValid() && bResolvedActors && bAuthoritySnapshotValid,
		FString::Printf(TEXT("QTE created. Handle=%d CellCount=%d"), CurrentHandle.GetValue(), CurrentCellContents.Num()));

	if (!CurrentHandle.IsValid() || !bResolvedActors || !bAuthoritySnapshotValid)
	{
		FailTest(TEXT("QTE failed to create required runtime actors or authority state."));
		return;
	}

	TriggerStep(ECommonQTETestStep::VerifyInitialState);
}

void UCommonQTEFlowTestRunner::EnterWaitingForInput()
{
	bInputInProgress = false;
	PendingStep = ECommonQTETestStep::None;
	Report.CurrentStep = ECommonQTETestStep::WaitingForInput;
	Report.Summary = TEXT("CommonQTE test is waiting for external input.");
	BroadcastReport();
	PrintVisualMessage(Report.Summary, false);
}

void UCommonQTEFlowTestRunner::CompleteInputCycle()
{
	ResetInputEventWaits();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	if (bAuthoritySnapshotValid && IsTerminalState(AuthoritySnapshot.QTEState) && Config.bAutoFinishWhenTerminal)
	{
		bInputInProgress = false;
		FinishTest(true, TEXT("CommonQTE test reached terminal state."));
		return;
	}

	EnterWaitingForInput();
}

void UCommonQTEFlowTestRunner::HandleStepTimeout()
{
	FailTest(FString::Printf(TEXT("Step timed out: %s."), *GetStepName(PendingStep)));
}

void UCommonQTEFlowTestRunner::ExecuteStep(ECommonQTETestStep InStep)
{
	switch (InStep)
	{
	case ECommonQTETestStep::VerifyInitialState:
		VerifyInitialState();
		break;
	case ECommonQTETestStep::VerifyPredictionOnly:
		VerifyPredictionOnly();
		break;
	case ECommonQTETestStep::VerifyAuthorityApplied:
		VerifyAuthorityApplied();
		break;
	case ECommonQTETestStep::VerifyPerformerReconciled:
		VerifyPerformerReconciled();
		break;
	case ECommonQTETestStep::VerifyObserverApplied:
		VerifyObserverApplied();
		break;
	default:
		FailTest(FString::Printf(TEXT("Unsupported test step: %s."), *GetStepName(InStep)));
		break;
	}
}

void UCommonQTEFlowTestRunner::BeginWaitingForStep(ECommonQTETestStep InStep)
{
	if (Report.State != ECommonQTETestState::Running)
	{
		return;
	}

	PendingStep = InStep;
	Report.CurrentStep = InStep;
	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
		if (Config.StepTimeoutSeconds > 0.0f)
		{
			FTimerDelegate TimeoutDelegate;
			TimeoutDelegate.BindUObject(this, &UCommonQTEFlowTestRunner::HandleStepTimeout);
			CachedWorld->GetTimerManager().SetTimer(StepTimeoutTimerHandle, TimeoutDelegate, Config.StepTimeoutSeconds, false);
		}
	}
	BroadcastReport();
	TriggerObservedWaitingStep(InStep);
}

void UCommonQTEFlowTestRunner::TriggerStep(ECommonQTETestStep InStep)
{
	if (Report.State != ECommonQTETestState::Running)
	{
		return;
	}

	PendingStep = InStep;
	Report.CurrentStep = InStep;
	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
	}
	BroadcastReport();

	if (Config.VisualizationConfig.bPauseBetweenSteps)
	{
		bWaitingForManualContinue = true;
		PrintVisualMessage(FString::Printf(TEXT("Paused before step: %s."), *GetStepName(InStep)), false);
		return;
	}

	PendingStep = ECommonQTETestStep::None;
	ExecuteStep(InStep);
}

void UCommonQTEFlowTestRunner::TriggerWaitingStep(ECommonQTETestStep InStep)
{
	if (PendingStep != InStep)
	{
		return;
	}

	TriggerStep(InStep);
}

void UCommonQTEFlowTestRunner::TriggerObservedWaitingStep(ECommonQTETestStep InStep)
{
	if (PendingStep != InStep)
	{
		return;
	}

	switch (InStep)
	{
	case ECommonQTETestStep::VerifyPredictionOnly:
		if (bWaitingForPredictionApply && bPredictionApplyObserved)
		{
			bWaitingForPredictionApply = false;
			TriggerWaitingStep(InStep);
		}
		break;
	case ECommonQTETestStep::VerifyAuthorityApplied:
		if (bWaitingForAuthorityApply && bAuthorityApplyObserved)
		{
			bWaitingForAuthorityApply = false;
			TriggerWaitingStep(InStep);
		}
		break;
	case ECommonQTETestStep::VerifyPerformerReconciled:
		if (bWaitingForPerformerReconcile && bPerformerReconcileObserved)
		{
			bWaitingForPerformerReconcile = false;
			TriggerWaitingStep(InStep);
		}
		break;
	case ECommonQTETestStep::VerifyObserverApplied:
		if (bWaitingForObserverPresentationApply && IsObserverPresentationReadyForCurrentInput())
		{
			bWaitingForObserverPresentationApply = false;
			TriggerWaitingStep(InStep);
		}
		break;
	default:
		break;
	}
}

void UCommonQTEFlowTestRunner::VerifyInitialState()
{
	const bool bResolvedActors = ResolveRuntimeActors();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	if (bAuthoritySnapshotValid)
	{
		CurrentCellContents = AuthoritySnapshot.CellContents;
	}

	const bool bPerformerReady = CurrentPerformer.IsValid() && CurrentPerformer->GetTestTrace().bReceivedInitialState;
	const bool bObserverReady = !Config.bCreateObserver || (CurrentObserver.IsValid() && CurrentObserver->GetTestTrace().bReceivedInitialState);
	const bool bAuthorityOnGoing = bAuthoritySnapshotValid && AuthoritySnapshot.QTEState == ECommonQTEState::OnGoing;
	const bool bPerformerOnGoing = GetPerformerPredictedState() == ECommonQTEState::OnGoing;
	const bool bPassed = bResolvedActors && bAuthoritySnapshotValid && bPerformerReady && bObserverReady && bAuthorityOnGoing && bPerformerOnGoing;

	AddStepResult(
		ECommonQTETestStep::VerifyInitialState,
		bPassed,
		FString::Printf(
			TEXT("Initial state check. PerformerReady=%s ObserverReady=%s AuthorityOnGoing=%s PerformerOnGoing=%s CellCount=%d"),
			FCommonQTELog::BoolToString(bPerformerReady),
			FCommonQTELog::BoolToString(bObserverReady),
			FCommonQTELog::BoolToString(bAuthorityOnGoing),
			FCommonQTELog::BoolToString(bPerformerOnGoing),
			CurrentCellContents.Num()));

	if (!bPassed)
	{
		FailTest(TEXT("Initial state verification failed."));
		return;
	}

	EnterWaitingForInput();
}

void UCommonQTEFlowTestRunner::VerifyPredictionOnly()
{
	RefreshVisualState();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	const FCommonQTETestPerformerTrace PerformerTrace = CurrentPerformer.IsValid() ? CurrentPerformer->GetTestTrace() : FCommonQTETestPerformerTrace();
	const bool bPerformerApplied = PerformerTrace.PredictionApplyCount > LastInputRecord.PreInputPredictionApplyCount;
	const FCommonQTEStateSnapshot& StateAfterPrediction = PerformerTrace.LastStateAfterPrediction;
	const bool bPredictionSnapshotValid = bPerformerApplied && StateAfterPrediction.WholeHandle == CurrentHandle;
	const bool bPerformerChanged = !bHasPreInputPerformerSnapshot || (bPredictionSnapshotValid && !AreStateSnapshotsEquivalent(PreInputPerformerSnapshot, StateAfterPrediction));
	const ECommonQTECellState ExpectedLocalCellState = LastInputRecord.bLocalExpectedMatched ? ECommonQTECellState::Success : ECommonQTECellState::Fail;
	const bool bPerformerCellMatchedLocal = bPredictionSnapshotValid
		&& StateAfterPrediction.CellStates.IsValidIndex(LastInputRecord.CellIndex)
		&& StateAfterPrediction.CellStates[LastInputRecord.CellIndex] == ExpectedLocalCellState;
	const bool bShouldAuthorityStayIsolated = Config.NetworkSimulationConfig.bVerifyEndpointIsolation && GetPerformerToAuthorityDelayForTest() > 0.0f;
	const bool bAuthorityIsolated = !bShouldAuthorityStayIsolated || (bHasPreInputAuthoritySnapshot && bAuthoritySnapshotValid && AreStateSnapshotsEquivalent(PreInputAuthoritySnapshot, AuthoritySnapshot));
	const bool bShouldObserverStayIsolated = Config.NetworkSimulationConfig.bVerifyEndpointIsolation && Config.bCreateObserver && GetAuthorityToObserverDelayForTest() > 0.0f;
	const bool bObserverAppliedCurrentPrediction = VisualState.ObserverPresentationState.bHasState && VisualState.ObserverPresentationState.LastPredictionId == LastInputRecord.PredictionId;
	const bool bObserverIsolated = !bShouldObserverStayIsolated || !bObserverAppliedCurrentPrediction;
	const bool bPassed = bPredictionSnapshotValid && bPerformerChanged && bPerformerCellMatchedLocal && bAuthorityIsolated && bObserverIsolated;

	AddStepResult(
		ECommonQTETestStep::VerifyPredictionOnly,
		bPassed,
		FString::Printf(
			TEXT("Prediction endpoint check. Behavior=%s PredictionId=%d PredictionSnapshotValid=%s PerformerChanged=%s PerformerCellMatchedLocal=%s AuthorityIsolated=%s ObserverIsolated=%s"),
			*GetInputBehaviorName(LastInputRecord.Behavior),
			LastInputRecord.PredictionId,
			FCommonQTELog::BoolToString(bPredictionSnapshotValid),
			FCommonQTELog::BoolToString(bPerformerChanged),
			FCommonQTELog::BoolToString(bPerformerCellMatchedLocal),
			FCommonQTELog::BoolToString(bAuthorityIsolated),
			FCommonQTELog::BoolToString(bObserverIsolated)));

	if (!bPassed)
	{
		FailTest(TEXT("Prediction endpoint verification failed."));
		return;
	}

	bWaitingForAuthorityApply = true;
	BeginWaitingForStep(ECommonQTETestStep::VerifyAuthorityApplied);
}

void UCommonQTEFlowTestRunner::VerifyAuthorityApplied()
{
	RefreshVisualState();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	FCommonQTEStateSnapshot PerformerSnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	const bool bPerformerSnapshotValid = GetPerformerSnapshot(PerformerSnapshot);
	const bool bAuthorityChanged = !bHasPreInputAuthoritySnapshot || (bAuthoritySnapshotValid && !AreStateSnapshotsEquivalent(PreInputAuthoritySnapshot, AuthoritySnapshot));
	const ECommonQTECellState ExpectedAuthorityCellState = LastInputRecord.bAuthorityExpectedMatched ? ECommonQTECellState::Success : ECommonQTECellState::Fail;
	const bool bAuthorityCellMatchedOutbound = bAuthoritySnapshotValid
		&& AuthoritySnapshot.CellStates.IsValidIndex(LastInputRecord.CellIndex)
		&& AuthoritySnapshot.CellStates[LastInputRecord.CellIndex] == ExpectedAuthorityCellState;
	const bool bAuthorityMatchesPerformer = bAuthoritySnapshotValid && bPerformerSnapshotValid && AreStateSnapshotsEquivalent(AuthoritySnapshot, PerformerSnapshot);
	const bool bCheatDivergedBeforeRollback = LastInputRecord.Behavior != ECommonQTETestInputBehavior::PerformerCheatWrongContentButLocalCorrect
		|| GetAuthorityToPerformerDelayForTest() <= 0.0f
		|| !bAuthorityMatchesPerformer;
	const bool bPassed = bAuthoritySnapshotValid && bAuthorityChanged && bAuthorityCellMatchedOutbound && bCheatDivergedBeforeRollback;

	AddStepResult(
		ECommonQTETestStep::VerifyAuthorityApplied,
		bPassed,
		FString::Printf(
			TEXT("Authority endpoint check. Behavior=%s PredictionId=%d AuthorityChanged=%s AuthorityCellMatchedOutbound=%s AuthorityMatchesPerformer=%s CheatDivergedBeforeRollback=%s"),
			*GetInputBehaviorName(LastInputRecord.Behavior),
			LastInputRecord.PredictionId,
			FCommonQTELog::BoolToString(bAuthorityChanged),
			FCommonQTELog::BoolToString(bAuthorityCellMatchedOutbound),
			FCommonQTELog::BoolToString(bAuthorityMatchesPerformer),
			FCommonQTELog::BoolToString(bCheatDivergedBeforeRollback)));

	if (!bPassed)
	{
		FailTest(TEXT("Authority endpoint verification failed."));
		return;
	}

	const bool bAuthoritativeSyncExpected = !LastInputRecord.bAuthorityExpectedMatched || (bAuthoritySnapshotValid && IsTerminalState(AuthoritySnapshot.QTEState));
	if (bAuthoritativeSyncExpected)
	{
		bWaitingForPerformerReconcile = true;
		BeginWaitingForStep(ECommonQTETestStep::VerifyPerformerReconciled);
		return;
	}

	TriggerStep(ECommonQTETestStep::VerifyPerformerReconciled);
}

void UCommonQTEFlowTestRunner::VerifyPerformerReconciled()
{
	RefreshVisualState();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	FCommonQTEStateSnapshot PerformerSnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	const bool bPerformerSnapshotValid = GetPerformerSnapshot(PerformerSnapshot);
	const bool bPerformerMatchesAuthority = bAuthoritySnapshotValid && bPerformerSnapshotValid && AreStateSnapshotsEquivalent(AuthoritySnapshot, PerformerSnapshot);
	const bool bAuthoritativeSyncExpected = !LastInputRecord.bAuthorityExpectedMatched || (bAuthoritySnapshotValid && IsTerminalState(AuthoritySnapshot.QTEState));
	const bool bAuthoritativeSyncApplied = !bAuthoritativeSyncExpected
		|| (CurrentPerformer.IsValid() && CurrentPerformer->GetTestTrace().RollbackApplyCount > LastInputRecord.PreInputRollbackApplyCount);
	const bool bPassed = bAuthoritySnapshotValid && bPerformerSnapshotValid && bPerformerMatchesAuthority && bAuthoritativeSyncApplied;

	AddStepResult(
		ECommonQTETestStep::VerifyPerformerReconciled,
		bPassed,
		FString::Printf(
			TEXT("Performer reconciliation check. Behavior=%s PredictionId=%d PerformerMatchesAuthority=%s AuthoritativeSyncExpected=%s AuthoritativeSyncApplied=%s"),
			*GetInputBehaviorName(LastInputRecord.Behavior),
			LastInputRecord.PredictionId,
			FCommonQTELog::BoolToString(bPerformerMatchesAuthority),
			FCommonQTELog::BoolToString(bAuthoritativeSyncExpected),
			FCommonQTELog::BoolToString(bAuthoritativeSyncApplied)));

	if (!bPassed)
	{
		FailTest(TEXT("Performer reconciliation verification failed."));
		return;
	}

	if (Config.bCreateObserver)
	{
		if (IsObserverPresentationReadyForCurrentInput())
		{
			TriggerStep(ECommonQTETestStep::VerifyObserverApplied);
			return;
		}

		bWaitingForObserverPresentationApply = true;
		BeginWaitingForStep(ECommonQTETestStep::VerifyObserverApplied);
		return;
	}

	CompleteInputCycle();
}

void UCommonQTEFlowTestRunner::VerifyObserverApplied()
{
	RefreshVisualState();
	FCommonQTEStateSnapshot AuthoritySnapshot;
	const bool bAuthoritySnapshotValid = GetAuthoritySnapshot(AuthoritySnapshot);
	const bool bObserverMatchesAuthority = !Config.bCreateObserver || (VisualState.bObserverPresentationMatchesAuthority && DoesPresentationStateMatchSnapshot(VisualState.ObserverPresentationState, AuthoritySnapshot));
	const bool bPassed = bAuthoritySnapshotValid && bObserverMatchesAuthority;

	AddStepResult(
		ECommonQTETestStep::VerifyObserverApplied,
		bPassed,
		FString::Printf(
			TEXT("Observer endpoint check. Behavior=%s PredictionId=%d AuthoritySnapshotValid=%s ObserverMatchesAuthority=%s PendingObserverMessages=%d"),
			*GetInputBehaviorName(LastInputRecord.Behavior),
			LastInputRecord.PredictionId,
			FCommonQTELog::BoolToString(bAuthoritySnapshotValid),
			FCommonQTELog::BoolToString(bObserverMatchesAuthority),
			GetPendingObserverPresentationMessageCount()));

	if (!bPassed)
	{
		FailTest(TEXT("Observer endpoint verification failed."));
		return;
	}

	CompleteInputCycle();
}

void UCommonQTEFlowTestRunner::FinishTest(bool bPassed, const FString& Summary)
{
	if (Report.State != ECommonQTETestState::Running && Report.State != ECommonQTETestState::NotStarted)
	{
		return;
	}

	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(StepTimeoutTimerHandle);
		CachedWorld->GetTimerManager().ClearTimer(NetworkSimulationTimerHandle);
	}

	Report.State = bPassed ? ECommonQTETestState::Passed : ECommonQTETestState::Failed;
	Report.bPassed = bPassed;
	Report.Summary = Summary;
	Report.CurrentStep = ECommonQTETestStep::Finished;
	PendingStep = ECommonQTETestStep::None;
	bInputInProgress = false;
	ResetInputEventWaits();
	UnbindManagerPresentationEvent();
	DelayedObserverPresentationBatches.Reset();
	BroadcastReport();
	PrintVisualMessage(Summary, !bPassed);
	OnTestFinished.Broadcast(Report);
	if (Config.bAutoStopWhenFinished)
	{
		CleanupCurrentQTE();
	}
	TeardownVisualization(false);

	if (!ShouldKeepRunnerForVisualization())
	{
		UnbindWorldLifecycle();
		if (bAddedToRootForAsyncRun)
		{
			RemoveFromRoot();
			bAddedToRootForAsyncRun = false;
		}
	}
}

void UCommonQTEFlowTestRunner::FailTest(const FString& Message)
{
	if (Report.State == ECommonQTETestState::Failed || Report.State == ECommonQTETestState::Passed || Report.State == ECommonQTETestState::Cancelled)
	{
		return;
	}

	if (Report.State == ECommonQTETestState::Running)
	{
		AddStepResult(Report.CurrentStep, false, Message);
	}

	FinishTest(false, Message);
}

void UCommonQTEFlowTestRunner::AddStepResult(ECommonQTETestStep Step, bool bPassed, const FString& Message)
{
	FCommonQTETestStepResult StepResult;
	StepResult.Step = Step;
	StepResult.InputBehavior = LastInputRecord.Behavior;
	StepResult.bPassed = bPassed;
	StepResult.Message = Message;
	StepResult.Handle = CurrentHandle;
	StepResult.AuthorityState = GetAuthorityState();
	StepResult.PerformerPredictedState = GetPerformerPredictedState();
	StepResult.PredictionId = LastInputRecord.PredictionId;
	StepResult.CellIndex = LastInputRecord.CellIndex;
	StepResult.LocalCellContent = LastInputRecord.LocalCellContent;
	StepResult.OutboundCellContent = LastInputRecord.OutboundCellContent;
	if (CurrentPerformer.IsValid())
	{
		const FCommonQTETestPerformerTrace& PerformerTrace = CurrentPerformer->GetTestTrace();
		StepResult.PredictionCount = PerformerTrace.PredictionApplyCount;
		StepResult.RollbackCount = PerformerTrace.RollbackApplyCount;
		Report.PredictionCount = PerformerTrace.PredictionApplyCount;
		Report.RollbackCount = PerformerTrace.RollbackApplyCount;
	}
	if (CurrentObserver.IsValid())
	{
		StepResult.ObserverDeltaCount = CurrentObserver->GetTestTrace().StateDeltaCount;
		Report.ObserverDeltaCount = StepResult.ObserverDeltaCount;
	}
	StepResult.PresentationMessageCount = Report.PresentationMessageCount;
	Report.StepResults.Add(StepResult);

	Report.CurrentStep = Step;
	BroadcastReport();
	PrintVisualMessage(Message, !bPassed);
	OnTestStepFinished.Broadcast(StepResult);
}

void UCommonQTEFlowTestRunner::BroadcastReport()
{
	RefreshVisualState();
	if (ResultWidget != nullptr)
	{
		ResultWidget->OnTestReportUpdated(Report);
	}
}

void UCommonQTEFlowTestRunner::PrintVisualMessage(const FString& Message, bool bError) const
{
	if (Config.VisualizationConfig.bLogToOutputLog)
	{
		UE_LOG(LogCommonQTETest, Log, TEXT("[CommonQTE Test] %s"), *Message);
	}

	if (Config.VisualizationConfig.bPrintScreenLog && GEngine != nullptr)
	{
		const FColor MessageColor = bError ? FColor::Red : FColor::Green;
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, MessageColor, FString::Printf(TEXT("[CommonQTE Test] %s"), *Message));
	}
}

void UCommonQTEFlowTestRunner::RefreshVisualState()
{
	VisualState.Handle = CurrentHandle;
	VisualState.Step = Report.CurrentStep;
	VisualState.LastInputBehavior = LastInputRecord.Behavior;
	VisualState.NetworkTopology = IsNetworkSimulationEnabled() ? Config.NetworkSimulationConfig.Topology : ECommonQTETestNetworkTopology::None;
	VisualState.ListenServerLayout = Config.NetworkSimulationConfig.ListenServerLayout;
	VisualState.bInputInProgress = bInputInProgress;
	VisualState.bObserverCreated = CurrentObserver.IsValid();
	VisualState.bHasAuthoritySnapshot = GetAuthoritySnapshot(VisualState.AuthoritySnapshot);
	VisualState.bHasPerformerSnapshot = GetPerformerSnapshot(VisualState.PerformerSnapshot);
	VisualState.CurrentCellIndex = GetCurrentAuthorityCellIndex();
	VisualState.CurrentExpectedCellContent = GetExpectedCorrectInput();
	VisualState.LastPredictionId = LastInputRecord.PredictionId;
	VisualState.LastLocalCellContent = LastInputRecord.LocalCellContent;
	VisualState.LastOutboundCellContent = LastInputRecord.OutboundCellContent;

	Report.CurrentCellIndex = VisualState.CurrentCellIndex;
	Report.CurrentExpectedCellContent = VisualState.CurrentExpectedCellContent;

	VisualState.AuthorityEndpointState = FCommonQTETestEndpointState();
	VisualState.AuthorityEndpointState.bHasSnapshot = VisualState.bHasAuthoritySnapshot;
	VisualState.AuthorityEndpointState.Snapshot = VisualState.AuthoritySnapshot;
	VisualState.AuthorityEndpointState.PendingPacketCount = CurrentPerformer.IsValid() ? CurrentPerformer->GetPendingDelayedPredictionMessageCount() : 0;

	VisualState.PerformerEndpointState = FCommonQTETestEndpointState();
	VisualState.PerformerEndpointState.bHasSnapshot = VisualState.bHasPerformerSnapshot;
	VisualState.PerformerEndpointState.Snapshot = VisualState.PerformerSnapshot;
	VisualState.PerformerEndpointState.LastPredictionId = LastInputRecord.PredictionId;
	VisualState.PerformerEndpointState.PendingPacketCount = CurrentPerformer.IsValid() ? CurrentPerformer->GetPendingDelayedRollBackMessageCount() : 0;

	VisualState.ObserverEndpointState = FCommonQTETestEndpointState();
	VisualState.ObserverEndpointState.bHasSnapshot = BuildObserverSnapshot(VisualState.ObserverEndpointState.Snapshot);
	VisualState.ObserverEndpointState.LastPredictionId = VisualState.ObserverPresentationState.LastPredictionId;
	VisualState.ObserverEndpointState.LastSequenceId = VisualState.ObserverPresentationState.LastSequenceId;
	VisualState.ObserverEndpointState.PendingPacketCount = GetPendingObserverPresentationMessageCount();

	VisualState.bPerformerPresentationMatchesPerformer = VisualState.bHasPerformerSnapshot && DoesPresentationStateMatchSnapshot(VisualState.PerformerPresentationState, VisualState.PerformerSnapshot);
	VisualState.bObserverPresentationMatchesAuthority = VisualState.bHasAuthoritySnapshot && DoesPresentationStateMatchSnapshot(VisualState.ObserverPresentationState, VisualState.AuthoritySnapshot);
}

void UCommonQTEFlowTestRunner::ResetVisualStateForCurrentQTE()
{
	VisualState = FCommonQTEFlowTestVisualState();
	VisualState.Step = Report.CurrentStep;
	VisualState.Handle = CurrentHandle;
	ResetStateSnapshotCaches();
	ResetPreInputEndpointState();
	DelayedObserverPresentationBatches.Reset();
	if (HasValidWorld())
	{
		CachedWorld->GetTimerManager().ClearTimer(NetworkSimulationTimerHandle);
	}
}

void UCommonQTEFlowTestRunner::ResetStateSnapshotCaches()
{
	CachedAuthoritySnapshot = FCommonQTEStateSnapshot();
	CachedPerformerSnapshot = FCommonQTEStateSnapshot();
	bHasCachedAuthoritySnapshot = false;
	bHasCachedPerformerSnapshot = false;
}

void UCommonQTEFlowTestRunner::CacheAuthoritySnapshot(const FCommonQTEStateSnapshot& Snapshot)
{
	if (!Snapshot.WholeHandle.IsValid() || (CurrentHandle.IsValid() && Snapshot.WholeHandle != CurrentHandle))
	{
		return;
	}

	CachedAuthoritySnapshot = Snapshot;
	bHasCachedAuthoritySnapshot = true;
}

void UCommonQTEFlowTestRunner::CachePerformerSnapshot(const FCommonQTEStateSnapshot& Snapshot)
{
	if (!Snapshot.WholeHandle.IsValid() || (CurrentHandle.IsValid() && Snapshot.WholeHandle != CurrentHandle))
	{
		return;
	}

	CachedPerformerSnapshot = Snapshot;
	bHasCachedPerformerSnapshot = true;
}

void UCommonQTEFlowTestRunner::CapturePreInputEndpointState()
{
	ResetPreInputEndpointState();
	bHasPreInputAuthoritySnapshot = GetAuthoritySnapshot(PreInputAuthoritySnapshot);
	bHasPreInputPerformerSnapshot = GetPerformerSnapshot(PreInputPerformerSnapshot);
}

void UCommonQTEFlowTestRunner::ResetPreInputEndpointState()
{
	PreInputAuthoritySnapshot = FCommonQTEStateSnapshot();
	PreInputPerformerSnapshot = FCommonQTEStateSnapshot();
	bHasPreInputAuthoritySnapshot = false;
	bHasPreInputPerformerSnapshot = false;
}

void UCommonQTEFlowTestRunner::ApplyPresentationMessageToState(const FCommonQTEPresentationMessage& Message, FCommonQTEPresentationVisualState& VisualPresentationState)
{
	if (!VisualPresentationState.bHasState || VisualPresentationState.Handle != Message.WholeHandle)
	{
		VisualPresentationState = FCommonQTEPresentationVisualState();
		VisualPresentationState.Handle = Message.WholeHandle;
		VisualPresentationState.bHasState = Message.WholeHandle.IsValid();
	}

	VisualPresentationState.LastSource = Message.Source;
	VisualPresentationState.LastPhase = Message.Phase;
	VisualPresentationState.QTEState = Message.QTEState;
	VisualPresentationState.LastChangedCellIndex = Message.CellIndex;
	VisualPresentationState.LastSequenceId = Message.SequenceId;
	VisualPresentationState.LastPredictionId = Message.PredictionId;
	++VisualPresentationState.MessageCount;

	if (Message.CellIndex == INDEX_NONE)
	{
		return;
	}

	if (VisualPresentationState.CellContents.Num() <= Message.CellIndex)
	{
		VisualPresentationState.CellContents.SetNum(Message.CellIndex + 1);
	}
	if (VisualPresentationState.CellStates.Num() <= Message.CellIndex)
	{
		const int32 OldCount = VisualPresentationState.CellStates.Num();
		VisualPresentationState.CellStates.SetNum(Message.CellIndex + 1);
		for (int32 CellStateIndex = OldCount; CellStateIndex < VisualPresentationState.CellStates.Num(); ++CellStateIndex)
		{
			VisualPresentationState.CellStates[CellStateIndex] = ECommonQTECellState::OnGoing;
		}
	}

	VisualPresentationState.CellContents[Message.CellIndex] = Message.CellContent;
	VisualPresentationState.CellStates[Message.CellIndex] = Message.CellState;
}

void UCommonQTEFlowTestRunner::ApplyObserverPresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages)
{
	for (const FCommonQTEPresentationMessage& Message : Messages)
	{
		ApplyPresentationMessageToState(Message, VisualState.ObserverPresentationState);
	}

	if (bWaitingForObserverPresentationApply && IsObserverPresentationReadyForCurrentInput())
	{
		bWaitingForObserverPresentationApply = false;
		TriggerWaitingStep(ECommonQTETestStep::VerifyObserverApplied);
	}
}

bool UCommonQTEFlowTestRunner::IsObserverPresentationReadyForCurrentInput()
{
	if (!Config.bCreateObserver)
	{
		return true;
	}

	if (!VisualState.ObserverPresentationState.bHasState || VisualState.ObserverPresentationState.Handle != CurrentHandle)
	{
		return false;
	}

	if (LastInputRecord.ExpectedObserverSequenceId > 0)
	{
		return VisualState.ObserverPresentationState.LastSequenceId >= LastInputRecord.ExpectedObserverSequenceId;
	}

	FCommonQTEStateSnapshot AuthoritySnapshot;
	return GetAuthoritySnapshot(AuthoritySnapshot) && DoesPresentationStateMatchSnapshot(VisualState.ObserverPresentationState, AuthoritySnapshot);
}

void UCommonQTEFlowTestRunner::QueueObserverPresentationMessages(const TArray<FCommonQTEPresentationMessage>& Messages)
{
	if (Report.State != ECommonQTETestState::Running || Messages.IsEmpty())
	{
		return;
	}

	if (!HasValidWorld())
	{
		ApplyObserverPresentationMessages(Messages);
		return;
	}

	FCommonQTEDelayedPresentationMessageBatch Batch;
	Batch.Messages = Messages;
	Batch.DeliveryTimeSeconds = CachedWorld->GetTimeSeconds() + GetAuthorityToObserverDelayForTest();
	DelayedObserverPresentationBatches.Add(Batch);
	ScheduleNetworkSimulationDrain();
}

void UCommonQTEFlowTestRunner::ScheduleNetworkSimulationDrain()
{
	if (Report.State != ECommonQTETestState::Running || !HasValidWorld() || DelayedObserverPresentationBatches.IsEmpty())
	{
		return;
	}

	const double NextDeliveryTime = DelayedObserverPresentationBatches[0].DeliveryTimeSeconds;

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UCommonQTEFlowTestRunner::DrainNetworkSimulationQueues);
	const float DelaySeconds = FMath::Max(0.0f, static_cast<float>(NextDeliveryTime - CachedWorld->GetTimeSeconds()));
	if (DelaySeconds > 0.0f)
	{
		CachedWorld->GetTimerManager().SetTimer(NetworkSimulationTimerHandle, TimerDelegate, DelaySeconds, false);
	}
	else
	{
		NetworkSimulationTimerHandle = CachedWorld->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	}
}

void UCommonQTEFlowTestRunner::DrainNetworkSimulationQueues()
{
	if (Report.State != ECommonQTETestState::Running || !HasValidWorld())
	{
		DelayedObserverPresentationBatches.Reset();
		return;
	}

	const double CurrentTime = CachedWorld->GetTimeSeconds();
	while (Report.State == ECommonQTETestState::Running && !DelayedObserverPresentationBatches.IsEmpty())
	{
		if (DelayedObserverPresentationBatches[0].DeliveryTimeSeconds > CurrentTime)
		{
			break;
		}

		const TArray<FCommonQTEPresentationMessage> Messages = DelayedObserverPresentationBatches[0].Messages;
		DelayedObserverPresentationBatches.RemoveAt(0, 1, EAllowShrinking::No);
		ApplyObserverPresentationMessages(Messages);
	}

	if (Report.State != ECommonQTETestState::Running)
	{
		DelayedObserverPresentationBatches.Reset();
		return;
	}

	BroadcastReport();
	ScheduleNetworkSimulationDrain();
}

void UCommonQTEFlowTestRunner::CleanupCurrentQTE()
{
	UnbindPerformerRollBackEvent();

	if (Manager.IsValid() && CurrentHandle.IsValid())
	{
		Manager->RemoveEntity(CurrentHandle);
	}

	CurrentHandle.Reset();
	Report.CurrentHandle.Reset();
	CurrentPerformer.Reset();
	CurrentObserver.Reset();
}

void UCommonQTEFlowTestRunner::BindManagerPresentationEvent()
{
	if (!Manager.IsValid() || bPresentationEventBound)
	{
		return;
	}

	Manager->OnPresentationMessagesReady.AddDynamic(this, &UCommonQTEFlowTestRunner::HandlePresentationMessagesReady);
	Manager->OnPerformerVerificationApplied.AddDynamic(this, &UCommonQTEFlowTestRunner::HandlePerformerVerificationApplied);
	bPresentationEventBound = true;
}

void UCommonQTEFlowTestRunner::UnbindManagerPresentationEvent()
{
	if (!bPresentationEventBound)
	{
		return;
	}

	if (Manager.IsValid())
	{
		Manager->OnPresentationMessagesReady.RemoveDynamic(this, &UCommonQTEFlowTestRunner::HandlePresentationMessagesReady);
		Manager->OnPerformerVerificationApplied.RemoveDynamic(this, &UCommonQTEFlowTestRunner::HandlePerformerVerificationApplied);
	}
	bPresentationEventBound = false;
}

void UCommonQTEFlowTestRunner::BindPerformerRollBackEvent()
{
	if (!CurrentPerformer.IsValid())
	{
		return;
	}

	if (BoundRollBackEventPerformer.Get() == CurrentPerformer.Get())
	{
		return;
	}

	UnbindPerformerRollBackEvent();
	CurrentPerformer->OnLocalPredictionMessagesReady.AddDynamic(this, &UCommonQTEFlowTestRunner::HandleLocalPredictionMessagesReady);
	CurrentPerformer->OnRollBackMessagesReady.AddDynamic(this, &UCommonQTEFlowTestRunner::HandlePerformerRollBackMessagesReady);
	BoundRollBackEventPerformer = CurrentPerformer;
}

void UCommonQTEFlowTestRunner::UnbindPerformerRollBackEvent()
{
	if (BoundRollBackEventPerformer.IsValid())
	{
		BoundRollBackEventPerformer->OnLocalPredictionMessagesReady.RemoveDynamic(this, &UCommonQTEFlowTestRunner::HandleLocalPredictionMessagesReady);
		BoundRollBackEventPerformer->OnRollBackMessagesReady.RemoveDynamic(this, &UCommonQTEFlowTestRunner::HandlePerformerRollBackMessagesReady);
	}
	BoundRollBackEventPerformer.Reset();
}

void UCommonQTEFlowTestRunner::ResetInputEventWaits()
{
	bWaitingForPredictionApply = false;
	bWaitingForAuthorityApply = false;
	bWaitingForPerformerReconcile = false;
	bWaitingForObserverPresentationApply = false;
	bPredictionApplyObserved = false;
	bAuthorityApplyObserved = false;
	bPerformerReconcileObserved = false;
}

bool UCommonQTEFlowTestRunner::DoesPredictionEventMatchCurrentInput(const FCommonQTEPerformerPredictionMessage& Message) const
{
	if (!bInputInProgress || !CurrentHandle.IsValid() || Message.WholeHandle != CurrentHandle)
	{
		return false;
	}

	if (LastInputRecord.PredictionId > 0)
	{
		return Message.PredictionId == LastInputRecord.PredictionId;
	}

	return Message.PredictionId > 0 && Message.CellIndex == LastInputRecord.CellIndex;
}

bool UCommonQTEFlowTestRunner::DoesRollBackEventMatchCurrentInput(const FCommonQTEPerformerRollBackMessage& Message) const
{
	if (!bInputInProgress || !CurrentHandle.IsValid() || Message.WholeHandle != CurrentHandle)
	{
		return false;
	}

	if (Message.PredictionId == 0)
	{
		return LastInputRecord.PredictionId > 0;
	}

	if (LastInputRecord.PredictionId > 0)
	{
		return Message.PredictionId == LastInputRecord.PredictionId;
	}

	return false;
}

void UCommonQTEFlowTestRunner::RecordCurrentPredictionId(int32 PredictionId)
{
	if (PredictionId <= 0)
	{
		return;
	}

	if (LastInputRecord.PredictionId <= 0)
	{
		LastInputRecord.PredictionId = PredictionId;
		Report.LastPredictionId = PredictionId;
	}
}

bool UCommonQTEFlowTestRunner::ResolveRuntimeActors()
{
	if (!Manager.IsValid() || !CurrentHandle.IsValid())
	{
		return false;
	}

	UnbindPerformerRollBackEvent();
	CurrentPerformer.Reset();
	for (ACommonQTEPerformerActor* Performer : Manager->GetPerformers(CurrentHandle))
	{
		if (ACommonQTETestPerformerActor* TestPerformer = Cast<ACommonQTETestPerformerActor>(Performer))
		{
			CurrentPerformer = TestPerformer;
			CurrentPerformer->SetNetworkSimulationConfig(Config.NetworkSimulationConfig);
			BindPerformerRollBackEvent();
			break;
		}
	}

	CurrentObserver = Cast<ACommonQTETestObserverActor>(Manager->GetObserver(CurrentHandle));
	if (CurrentObserver.IsValid())
	{
		CurrentObserver->SetForcePresentationRoutingForTest(Config.bUseTestObserverForceRouting);
	}

	return CurrentPerformer.IsValid() && (!Config.bCreateObserver || CurrentObserver.IsValid());
}

bool UCommonQTEFlowTestRunner::HasValidWorld() const
{
	return CachedWorld.IsValid();
}

bool UCommonQTEFlowTestRunner::GetAuthoritySnapshot(FCommonQTEStateSnapshot& OutSnapshot)
{
	if (Manager.IsValid() && CurrentHandle.IsValid() && Manager->GetEntityStateSnapshot(CurrentHandle, OutSnapshot))
	{
		CacheAuthoritySnapshot(OutSnapshot);
		return true;
	}

	if (CurrentPerformer.IsValid())
	{
		const FCommonQTETestPerformerTrace PerformerTrace = CurrentPerformer->GetTestTrace();
		if (PerformerTrace.LastStateAfterRollback.WholeHandle == CurrentHandle)
		{
			CacheAuthoritySnapshot(PerformerTrace.LastStateAfterRollback);
			OutSnapshot = CachedAuthoritySnapshot;
			return true;
		}
	}

	if (bHasCachedAuthoritySnapshot && CachedAuthoritySnapshot.WholeHandle == CurrentHandle)
	{
		OutSnapshot = CachedAuthoritySnapshot;
		return true;
	}

	OutSnapshot = FCommonQTEStateSnapshot();
	return false;
}

bool UCommonQTEFlowTestRunner::GetPerformerSnapshot(FCommonQTEStateSnapshot& OutSnapshot)
{
	if (CurrentPerformer.IsValid())
	{
		OutSnapshot = CurrentPerformer->GetPredictedStateSnapshot();
		if (OutSnapshot.WholeHandle.IsValid() && (!CurrentHandle.IsValid() || OutSnapshot.WholeHandle == CurrentHandle))
		{
			CachePerformerSnapshot(OutSnapshot);
			return true;
		}
	}

	if (bHasCachedPerformerSnapshot && CachedPerformerSnapshot.WholeHandle == CurrentHandle)
	{
		OutSnapshot = CachedPerformerSnapshot;
		return true;
	}

	OutSnapshot = FCommonQTEStateSnapshot();
	return false;
}

bool UCommonQTEFlowTestRunner::BuildObserverSnapshot(FCommonQTEStateSnapshot& OutSnapshot) const
{
	if (!VisualState.ObserverPresentationState.bHasState)
	{
		OutSnapshot = FCommonQTEStateSnapshot();
		return false;
	}

	OutSnapshot = FCommonQTEStateSnapshot();
	OutSnapshot.WholeHandle = VisualState.ObserverPresentationState.Handle;
	OutSnapshot.QTEState = VisualState.ObserverPresentationState.QTEState;
	OutSnapshot.CellContents = VisualState.ObserverPresentationState.CellContents;
	OutSnapshot.CellStates = VisualState.ObserverPresentationState.CellStates;
	return true;
}

bool UCommonQTEFlowTestRunner::AreStateSnapshotsEquivalent(const FCommonQTEStateSnapshot& Left, const FCommonQTEStateSnapshot& Right) const
{
	if (Left.WholeHandle != Right.WholeHandle || Left.QTEState != Right.QTEState)
	{
		return false;
	}

	if (Left.DriverState.CellIndex != Right.DriverState.CellIndex || Left.DriverState.ErrorCount != Right.DriverState.ErrorCount)
	{
		return false;
	}

	if (Left.CellContents.Num() != Right.CellContents.Num() || Left.CellStates.Num() != Right.CellStates.Num())
	{
		return false;
	}

	for (int32 CellIndex = 0; CellIndex < Left.CellContents.Num(); ++CellIndex)
	{
		if (Left.CellContents[CellIndex] != Right.CellContents[CellIndex])
		{
			return false;
		}
	}

	for (int32 CellIndex = 0; CellIndex < Left.CellStates.Num(); ++CellIndex)
	{
		if (Left.CellStates[CellIndex] != Right.CellStates[CellIndex])
		{
			return false;
		}
	}

	return true;
}

bool UCommonQTEFlowTestRunner::DoesPresentationStateMatchSnapshot(const FCommonQTEPresentationVisualState& PresentationState, const FCommonQTEStateSnapshot& Snapshot) const
{
	if (!PresentationState.bHasState || PresentationState.Handle != Snapshot.WholeHandle || PresentationState.QTEState != Snapshot.QTEState)
	{
		return false;
	}

	if (PresentationState.CellContents.Num() != Snapshot.CellContents.Num() || PresentationState.CellStates.Num() != Snapshot.CellStates.Num())
	{
		return false;
	}

	for (int32 CellIndex = 0; CellIndex < Snapshot.CellContents.Num(); ++CellIndex)
	{
		if (PresentationState.CellContents[CellIndex] != Snapshot.CellContents[CellIndex])
		{
			return false;
		}
	}

	for (int32 CellIndex = 0; CellIndex < Snapshot.CellStates.Num(); ++CellIndex)
	{
		if (PresentationState.CellStates[CellIndex] != Snapshot.CellStates[CellIndex])
		{
			return false;
		}
	}

	return true;
}

bool UCommonQTEFlowTestRunner::IsNetworkSimulationEnabled() const
{
	return Config.NetworkSimulationConfig.bEnableNetworkSimulation && Config.NetworkSimulationConfig.Topology != ECommonQTETestNetworkTopology::None;
}

bool UCommonQTEFlowTestRunner::IsListenServerHostPerformer() const
{
	return IsNetworkSimulationEnabled()
		&& Config.NetworkSimulationConfig.Topology == ECommonQTETestNetworkTopology::ListenServer
		&& Config.NetworkSimulationConfig.ListenServerLayout == ECommonQTETestListenServerLayout::HostAsPerformer;
}

bool UCommonQTEFlowTestRunner::IsTerminalState(ECommonQTEState State) const
{
	return State == ECommonQTEState::Success || State == ECommonQTEState::Fail;
}

float UCommonQTEFlowTestRunner::GetPerformerToAuthorityDelayForTest() const
{
	return IsNetworkSimulationEnabled() && !IsListenServerHostPerformer() ? Config.NetworkSimulationConfig.PerformerToAuthorityDelaySeconds : 0.0f;
}

float UCommonQTEFlowTestRunner::GetAuthorityToPerformerDelayForTest() const
{
	return IsNetworkSimulationEnabled() && !IsListenServerHostPerformer() ? Config.NetworkSimulationConfig.AuthorityToPerformerDelaySeconds : 0.0f;
}

float UCommonQTEFlowTestRunner::GetAuthorityToObserverDelayForTest() const
{
	return IsNetworkSimulationEnabled() ? Config.NetworkSimulationConfig.AuthorityToObserverDelaySeconds : 0.0f;
}

float UCommonQTEFlowTestRunner::GetVerificationSettleDelay() const
{
	return IsNetworkSimulationEnabled() ? Config.NetworkSimulationConfig.VerificationSettleSeconds : 0.0f;
}

int32 UCommonQTEFlowTestRunner::GetPendingObserverPresentationMessageCount() const
{
	int32 MessageCount = 0;
	for (const FCommonQTEDelayedPresentationMessageBatch& Batch : DelayedObserverPresentationBatches)
	{
		MessageCount += Batch.Messages.Num();
	}
	return MessageCount;
}

int32 UCommonQTEFlowTestRunner::GetCurrentAuthorityCellIndex() const
{
	if (CachedAuthoritySnapshot.WholeHandle == CurrentHandle)
	{
		return CachedAuthoritySnapshot.DriverState.CellIndex;
	}

	return INDEX_NONE;
}

int32 UCommonQTEFlowTestRunner::GetExpectedCorrectInput() const
{
	if (CachedAuthoritySnapshot.WholeHandle == CurrentHandle && CachedAuthoritySnapshot.CellContents.IsValidIndex(CachedAuthoritySnapshot.DriverState.CellIndex))
	{
		return CachedAuthoritySnapshot.CellContents[CachedAuthoritySnapshot.DriverState.CellIndex];
	}

	return 0;
}

int32 UCommonQTEFlowTestRunner::GetResolvedWrongInput(int32 ExpectedInput) const
{
	if (Config.WrongCellContent != ExpectedInput)
	{
		return Config.WrongCellContent;
	}

	return ExpectedInput + 1;
}

ECommonQTEState UCommonQTEFlowTestRunner::GetAuthorityState()
{
	FCommonQTEStateSnapshot AuthoritySnapshot;
	return GetAuthoritySnapshot(AuthoritySnapshot) ? AuthoritySnapshot.QTEState : ECommonQTEState::Fail;
}

ECommonQTEState UCommonQTEFlowTestRunner::GetPerformerPredictedState()
{
	FCommonQTEStateSnapshot PerformerSnapshot;
	return GetPerformerSnapshot(PerformerSnapshot) ? PerformerSnapshot.QTEState : ECommonQTEState::Fail;
}

FString UCommonQTEFlowTestRunner::GetStepName(ECommonQTETestStep Step) const
{
	switch (Step)
	{
	case ECommonQTETestStep::None:
		return TEXT("None");
	case ECommonQTETestStep::CreateQTE:
		return TEXT("CreateQTE");
	case ECommonQTETestStep::VerifyInitialState:
		return TEXT("VerifyInitialState");
	case ECommonQTETestStep::WaitingForInput:
		return TEXT("WaitingForInput");
	case ECommonQTETestStep::SubmitInput:
		return TEXT("SubmitInput");
	case ECommonQTETestStep::VerifyPredictionOnly:
		return TEXT("VerifyPredictionOnly");
	case ECommonQTETestStep::VerifyAuthorityApplied:
		return TEXT("VerifyAuthorityApplied");
	case ECommonQTETestStep::VerifyPerformerReconciled:
		return TEXT("VerifyPerformerReconciled");
	case ECommonQTETestStep::VerifyObserverApplied:
		return TEXT("VerifyObserverApplied");
	case ECommonQTETestStep::Finished:
		return TEXT("Finished");
	default:
		return TEXT("Unknown");
	}
}

FString UCommonQTEFlowTestRunner::GetInputBehaviorName(ECommonQTETestInputBehavior Behavior) const
{
	switch (Behavior)
	{
	case ECommonQTETestInputBehavior::PerformerCorrect:
		return TEXT("PerformerCorrect");
	case ECommonQTETestInputBehavior::PerformerWrong:
		return TEXT("PerformerWrong");
	case ECommonQTETestInputBehavior::PerformerCheatWrongContentButLocalCorrect:
		return TEXT("PerformerCheatWrongContentButLocalCorrect");
	default:
		return TEXT("Unknown");
	}
}
