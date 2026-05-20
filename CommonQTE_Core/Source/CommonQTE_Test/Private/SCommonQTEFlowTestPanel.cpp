#include "SCommonQTEFlowTestPanel.h"

#include "CommonQTEFlowTestRunner.h"
#include "SCommonQTEFlowTestVisualizer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SCommonQTEFlowTestPanel::Construct(const FArguments& InArgs)
{
	Runner = InArgs._Runner;
	bAllowManualInput = InArgs._bAllowManualInput;
	bShowControlPanel = InArgs._bShowControlPanel;
	bShowVisualizer = InArgs._bShowVisualizer;
	bShowAuthorityView = InArgs._bShowAuthorityView;
	bShowPerformerView = InArgs._bShowPerformerView;
	bShowObserverView = InArgs._bShowObserverView;
	VisualizerScale = FMath::Clamp(InArgs._VisualizerScale, 0.5f, 2.0f);

	TSharedRef<SOverlay> RootOverlay = SNew(SOverlay);
	if (bShowVisualizer)
	{
		RootOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SCommonQTEFlowTestVisualizer)
			.Runner(Runner.Get())
			.Scale(VisualizerScale)
			.bShowAuthorityView(bShowAuthorityView)
			.bShowPerformerView(bShowPerformerView)
			.bShowObserverView(bShowObserverView)
		];
	}

	if (bShowControlPanel)
	{
		RootOverlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(24.0f)
		[
			BuildControlPanel()
		];
	}

	ChildSlot
	[
		RootOverlay
	];
}

TSharedRef<SWidget> SCommonQTEFlowTestPanel::BuildControlPanel() const
{
	return
		SNew(SBox)
		.WidthOverride(GetPanelWidth())
		.HeightOverride(GetPanelHeight())
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.92f))
			.Padding(FMargin(24.0f, 22.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(GetTitleFont())
					.ColorAndOpacity(FLinearColor::White)
					.Text(FText::FromString(TEXT("CommonQTE Flow Test")))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 16.0f, 0.0f, 18.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.Font(GetReportFont())
						.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f, 1.0f))
						.AutoWrapText(true)
						.Text(this, &SCommonQTEFlowTestPanel::GetReportText)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(GetButtonHeight())
					[
						SNew(SButton)
						.ContentPadding(FMargin(18.0f, 8.0f))
						.OnClicked(this, &SCommonQTEFlowTestPanel::OnContinueClicked)
						[
							SNew(STextBlock)
							.Font(GetButtonFont())
							.Justification(ETextJustify::Center)
							.Text(FText::FromString(TEXT("Continue")))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(GetButtonHeight())
					[
						SNew(SButton)
						.ContentPadding(FMargin(18.0f, 8.0f))
						.IsEnabled(bAllowManualInput)
						.OnClicked(this, &SCommonQTEFlowTestPanel::OnCorrectInputClicked)
						[
							SNew(STextBlock)
							.Font(GetButtonFont())
							.Justification(ETextJustify::Center)
							.Text(FText::FromString(TEXT("Correct")))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(GetButtonHeight())
					[
						SNew(SButton)
						.ContentPadding(FMargin(18.0f, 8.0f))
						.IsEnabled(bAllowManualInput)
						.OnClicked(this, &SCommonQTEFlowTestPanel::OnWrongInputClicked)
						[
							SNew(STextBlock)
							.Font(GetButtonFont())
							.Justification(ETextJustify::Center)
							.Text(FText::FromString(TEXT("Wrong")))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(GetButtonHeight())
					[
						SNew(SButton)
						.ContentPadding(FMargin(18.0f, 8.0f))
						.IsEnabled(bAllowManualInput)
						.OnClicked(this, &SCommonQTEFlowTestPanel::OnCheatInputClicked)
						[
							SNew(STextBlock)
							.Font(GetButtonFont())
							.Justification(ETextJustify::Center)
							.Text(FText::FromString(TEXT("Cheat")))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(GetButtonHeight())
					[
						SNew(SButton)
						.ContentPadding(FMargin(18.0f, 8.0f))
						.OnClicked(this, &SCommonQTEFlowTestPanel::OnStopClicked)
						[
							SNew(STextBlock)
							.Font(GetButtonFont())
							.Justification(ETextJustify::Center)
							.Text(FText::FromString(TEXT("Stop")))
						]
					]
				]
			]
		]
	;
}

FSlateFontInfo SCommonQTEFlowTestPanel::GetTitleFont()
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 26);
}

FSlateFontInfo SCommonQTEFlowTestPanel::GetReportFont()
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 18);
}

FSlateFontInfo SCommonQTEFlowTestPanel::GetButtonFont()
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 22);
}

float SCommonQTEFlowTestPanel::GetPanelWidth()
{
	return 720.0f;
}

float SCommonQTEFlowTestPanel::GetPanelHeight()
{
	return 640.0f;
}

float SCommonQTEFlowTestPanel::GetButtonHeight()
{
	return 56.0f;
}

FText SCommonQTEFlowTestPanel::GetReportText() const
{
	const UCommonQTEFlowTestRunner* TestRunner = Runner.Get();
	return FText::FromString(TestRunner != nullptr ? TestRunner->GetSummaryText() : FString(TEXT("Runner is missing.")));
}

FReply SCommonQTEFlowTestPanel::OnContinueClicked() const
{
	if (UCommonQTEFlowTestRunner* TestRunner = Runner.Get())
	{
		TestRunner->ContinueTest();
	}
	return FReply::Handled();
}

FReply SCommonQTEFlowTestPanel::OnCorrectInputClicked() const
{
	if (UCommonQTEFlowTestRunner* TestRunner = Runner.Get())
	{
		TestRunner->SubmitInput(ECommonQTETestInputBehavior::PerformerCorrect);
	}
	return FReply::Handled();
}

FReply SCommonQTEFlowTestPanel::OnWrongInputClicked() const
{
	if (UCommonQTEFlowTestRunner* TestRunner = Runner.Get())
	{
		TestRunner->SubmitInput(ECommonQTETestInputBehavior::PerformerWrong);
	}
	return FReply::Handled();
}

FReply SCommonQTEFlowTestPanel::OnCheatInputClicked() const
{
	if (UCommonQTEFlowTestRunner* TestRunner = Runner.Get())
	{
		TestRunner->SubmitInput(ECommonQTETestInputBehavior::PerformerCheatWrongContentButLocalCorrect);
	}
	return FReply::Handled();
}

FReply SCommonQTEFlowTestPanel::OnStopClicked() const
{
	if (UCommonQTEFlowTestRunner* TestRunner = Runner.Get())
	{
		TestRunner->CancelTest();
	}
	return FReply::Handled();
}
