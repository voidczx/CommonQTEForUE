#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UCommonQTEFlowTestRunner;

class SCommonQTEFlowTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCommonQTEFlowTestPanel)
		: _Runner(nullptr)
		, _bAllowManualInput(false)
		, _bShowControlPanel(true)
		, _bShowVisualizer(true)
		, _bShowAuthorityView(true)
		, _bShowPerformerView(true)
		, _bShowObserverView(true)
		, _VisualizerScale(1.0f)
	{
	}
		SLATE_ARGUMENT(UCommonQTEFlowTestRunner*, Runner)
		SLATE_ARGUMENT(bool, bAllowManualInput)
		SLATE_ARGUMENT(bool, bShowControlPanel)
		SLATE_ARGUMENT(bool, bShowVisualizer)
		SLATE_ARGUMENT(bool, bShowAuthorityView)
		SLATE_ARGUMENT(bool, bShowPerformerView)
		SLATE_ARGUMENT(bool, bShowObserverView)
		SLATE_ARGUMENT(float, VisualizerScale)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	static FSlateFontInfo GetTitleFont();
	static FSlateFontInfo GetReportFont();
	static FSlateFontInfo GetButtonFont();
	static float GetPanelWidth();
	static float GetPanelHeight();
	static float GetButtonHeight();

	TSharedRef<SWidget> BuildControlPanel() const;
	FText GetReportText() const;
	FReply OnContinueClicked() const;
	FReply OnCorrectInputClicked() const;
	FReply OnWrongInputClicked() const;
	FReply OnCheatInputClicked() const;
	FReply OnStopClicked() const;

	TWeakObjectPtr<UCommonQTEFlowTestRunner> Runner;
	bool bAllowManualInput = false;
	bool bShowControlPanel = true;
	bool bShowVisualizer = true;
	bool bShowAuthorityView = true;
	bool bShowPerformerView = true;
	bool bShowObserverView = true;
	float VisualizerScale = 1.0f;
};
