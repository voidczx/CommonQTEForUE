#include "CommonQTETestObserverActor.h"

void ACommonQTETestObserverActor::SetForcePresentationRoutingForTest(bool bInForcePresentationRouting)
{
	bForcePresentationRoutingForTest = bInForcePresentationRouting;
}

FCommonQTETestObserverTrace ACommonQTETestObserverActor::GetTestTrace() const
{
	return TestTrace;
}

void ACommonQTETestObserverActor::ResetTestTrace()
{
	TestTrace = FCommonQTETestObserverTrace();
}

void ACommonQTETestObserverActor::SetInitialStateSnapshot(const FCommonQTEStateSnapshot& InInitialStateSnapshot)
{
	TestTrace.bReceivedInitialState = true;
	TestTrace.LastInitialStateSnapshot = InInitialStateSnapshot;
	Super::SetInitialStateSnapshot(InInitialStateSnapshot);
}

void ACommonQTETestObserverActor::PushStateDelta(const FCommonQTEObserverStateDelta& InStateDelta)
{
	++TestTrace.StateDeltaCount;
	TestTrace.LastStateDelta = InStateDelta;
	Super::PushStateDelta(InStateDelta);
}

void ACommonQTETestObserverActor::BuildPresentationMessagesFromInitialState()
{
	++TestTrace.InitialPresentationBuildCount;
	Super::BuildPresentationMessagesFromInitialState();
}

void ACommonQTETestObserverActor::BuildPresentationMessageFromStateDelta(const FCommonQTEObserverStateDelta& InStateDelta)
{
	++TestTrace.DeltaPresentationBuildCount;
	Super::BuildPresentationMessageFromStateDelta(InStateDelta);
}

bool ACommonQTETestObserverActor::ShouldRoutePresentationMessages() const
{
	return bForcePresentationRoutingForTest || Super::ShouldRoutePresentationMessages();
}
