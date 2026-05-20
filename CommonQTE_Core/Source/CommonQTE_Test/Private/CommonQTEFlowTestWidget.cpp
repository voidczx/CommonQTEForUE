#include "CommonQTEFlowTestWidget.h"

#include "CommonQTEFlowTestRunner.h"

void UCommonQTEFlowTestWidget::InitializeWithRunner(UCommonQTEFlowTestRunner* InRunner)
{
	Runner = InRunner;
}

UCommonQTEFlowTestRunner* UCommonQTEFlowTestWidget::GetRunner() const
{
	return Runner;
}
