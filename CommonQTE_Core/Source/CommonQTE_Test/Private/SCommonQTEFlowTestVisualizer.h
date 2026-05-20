#pragma once

#include "CoreMinimal.h"
#include "CommonQTETestType.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UCommonQTEFlowTestRunner;

class SCommonQTEFlowTestVisualizer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCommonQTEFlowTestVisualizer)
		: _Runner(nullptr)
		, _Scale(1.0f)
		, _bShowAuthorityView(true)
		, _bShowPerformerView(true)
		, _bShowObserverView(true)
	{
	}
		SLATE_ARGUMENT(UCommonQTEFlowTestRunner*, Runner)
		SLATE_ARGUMENT(float, Scale)
		SLATE_ARGUMENT(bool, bShowAuthorityView)
		SLATE_ARGUMENT(bool, bShowPerformerView)
		SLATE_ARGUMENT(bool, bShowObserverView)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	static FSlateFontInfo GetTitleFont(float Scale);
	static FSlateFontInfo GetLabelFont(float Scale);
	static FSlateFontInfo GetCellFont(float Scale);
	static FVector2D GetBaseSize();
	static FLinearColor GetQTEStateColor(ECommonQTEState State);
	static FLinearColor GetCellStateColor(ECommonQTECellState State);

	int32 PaintSnapshotRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, const FCommonQTEStateSnapshot& Snapshot, bool bHasSnapshot, bool bMatched, float Scale) const;
	int32 PaintPresentationRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, const FCommonQTEPresentationVisualState& PresentationState, int32 FocusCellIndex, bool bMatched, float Scale) const;
	int32 PaintCellRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, ECommonQTEState QTEState, const TArray<int32>& CellContents, const TArray<ECommonQTECellState>& CellStates, int32 FocusCellIndex, bool bHasState, bool bMatched, float Scale) const;
	void PaintBox(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const;
	void PaintOutline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color, float Thickness) const;
	void PaintText(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color) const;

	TWeakObjectPtr<UCommonQTEFlowTestRunner> Runner;
	float Scale = 1.0f;
	bool bShowAuthorityView = true;
	bool bShowPerformerView = true;
	bool bShowObserverView = true;
};
