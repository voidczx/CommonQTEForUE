#include "SCommonQTEFlowTestVisualizer.h"

#include "CommonQTEFlowTestRunner.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SCommonQTEFlowTestVisualizer::Construct(const FArguments& InArgs)
{
	Runner = InArgs._Runner;
	Scale = FMath::Clamp(InArgs._Scale, 0.5f, 2.0f);
	bShowAuthorityView = InArgs._bShowAuthorityView;
	bShowPerformerView = InArgs._bShowPerformerView;
	bShowObserverView = InArgs._bShowObserverView;
}

int32 SCommonQTEFlowTestVisualizer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FCommonQTEFlowTestVisualState VisualState = Runner.IsValid() ? Runner->GetVisualState() : FCommonQTEFlowTestVisualState();
	const float LocalScale = Scale;
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	int32 CurrentLayer = LayerId;

	PaintBox(AllottedGeometry, OutDrawElements, CurrentLayer++, FVector2D::ZeroVector, LocalSize, FLinearColor(0.01f, 0.012f, 0.016f, 0.84f));
	PaintOutline(AllottedGeometry, OutDrawElements, CurrentLayer++, FVector2D::ZeroVector, LocalSize, FLinearColor(0.35f, 0.50f, 0.70f, 0.9f), 2.0f * LocalScale);
	PaintText(AllottedGeometry, OutDrawElements, CurrentLayer++, FVector2D(34.0f, 24.0f) * LocalScale, TEXT("CommonQTE Visual State"), GetTitleFont(LocalScale), FLinearColor::White);

	const float RowGap = 22.0f * LocalScale;
	const float RowHeight = 138.0f * LocalScale;
	FVector2D RowPosition = FVector2D(34.0f, 82.0f) * LocalScale;

	if (bShowAuthorityView)
	{
		CurrentLayer = PaintSnapshotRow(AllottedGeometry, OutDrawElements, CurrentLayer, RowPosition, TEXT("AUTH"), VisualState.AuthoritySnapshot, VisualState.bHasAuthoritySnapshot, true, LocalScale);
		RowPosition.Y += RowHeight + RowGap;
	}

	if (bShowPerformerView)
	{
		const int32 PerformerFocusIndex = VisualState.bHasPerformerSnapshot ? VisualState.PerformerSnapshot.DriverState.CellIndex : VisualState.PerformerPresentationState.LastChangedCellIndex;
		CurrentLayer = PaintPresentationRow(AllottedGeometry, OutDrawElements, CurrentLayer, RowPosition, TEXT("PERF"), VisualState.PerformerPresentationState, PerformerFocusIndex, VisualState.bPerformerPresentationMatchesPerformer, LocalScale);
		RowPosition.Y += RowHeight + RowGap;
	}

	if (bShowObserverView)
	{
		CurrentLayer = PaintPresentationRow(AllottedGeometry, OutDrawElements, CurrentLayer, RowPosition, TEXT("OBSV"), VisualState.ObserverPresentationState, VisualState.ObserverPresentationState.LastChangedCellIndex, VisualState.bObserverPresentationMatchesAuthority, LocalScale);
	}

	return CurrentLayer;
}

FVector2D SCommonQTEFlowTestVisualizer::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return GetBaseSize() * Scale;
}

FSlateFontInfo SCommonQTEFlowTestVisualizer::GetTitleFont(float Scale)
{
	return FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(30.0f * Scale));
}

FSlateFontInfo SCommonQTEFlowTestVisualizer::GetLabelFont(float Scale)
{
	return FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(22.0f * Scale));
}

FSlateFontInfo SCommonQTEFlowTestVisualizer::GetCellFont(float Scale)
{
	return FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(34.0f * Scale));
}

FVector2D SCommonQTEFlowTestVisualizer::GetBaseSize()
{
	return FVector2D(1120.0f, 620.0f);
}

FLinearColor SCommonQTEFlowTestVisualizer::GetQTEStateColor(ECommonQTEState State)
{
	switch (State)
	{
	case ECommonQTEState::Success:
		return FLinearColor(0.04f, 0.30f, 0.12f, 0.84f);
	case ECommonQTEState::Fail:
		return FLinearColor(0.34f, 0.05f, 0.045f, 0.84f);
	case ECommonQTEState::Pause:
		return FLinearColor(0.30f, 0.24f, 0.04f, 0.84f);
	case ECommonQTEState::OnGoing:
	default:
		return FLinearColor(0.035f, 0.10f, 0.18f, 0.84f);
	}
}

FLinearColor SCommonQTEFlowTestVisualizer::GetCellStateColor(ECommonQTECellState State)
{
	switch (State)
	{
	case ECommonQTECellState::Success:
		return FLinearColor(0.05f, 0.58f, 0.22f, 1.0f);
	case ECommonQTECellState::Fail:
		return FLinearColor(0.72f, 0.06f, 0.05f, 1.0f);
	case ECommonQTECellState::OnGoing:
	default:
		return FLinearColor(0.12f, 0.16f, 0.22f, 1.0f);
	}
}

int32 SCommonQTEFlowTestVisualizer::PaintSnapshotRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, const FCommonQTEStateSnapshot& Snapshot, bool bHasSnapshot, bool bMatched, float LocalScale) const
{
	const int32 FocusCellIndex = bHasSnapshot ? Snapshot.DriverState.CellIndex : INDEX_NONE;
	return PaintCellRow(AllottedGeometry, OutDrawElements, LayerId, RowPosition, Label, Snapshot.QTEState, Snapshot.CellContents, Snapshot.CellStates, FocusCellIndex, bHasSnapshot, bMatched, LocalScale);
}

int32 SCommonQTEFlowTestVisualizer::PaintPresentationRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, const FCommonQTEPresentationVisualState& PresentationState, int32 FocusCellIndex, bool bMatched, float LocalScale) const
{
	return PaintCellRow(AllottedGeometry, OutDrawElements, LayerId, RowPosition, Label, PresentationState.QTEState, PresentationState.CellContents, PresentationState.CellStates, FocusCellIndex, PresentationState.bHasState, bMatched, LocalScale);
}

int32 SCommonQTEFlowTestVisualizer::PaintCellRow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& RowPosition, const FString& Label, ECommonQTEState QTEState, const TArray<int32>& CellContents, const TArray<ECommonQTECellState>& CellStates, int32 FocusCellIndex, bool bHasState, bool bMatched, float LocalScale) const
{
	const FVector2D RowSize = FVector2D(1052.0f, 138.0f) * LocalScale;
	const FVector2D LabelPosition = RowPosition + FVector2D(20.0f, 48.0f) * LocalScale;
	const FVector2D CellStart = RowPosition + FVector2D(148.0f, 28.0f) * LocalScale;
	const FVector2D CellSize = FVector2D(78.0f, 78.0f) * LocalScale;
	const float CellGap = 16.0f * LocalScale;
	int32 CurrentLayer = LayerId;

	PaintBox(AllottedGeometry, OutDrawElements, CurrentLayer++, RowPosition, RowSize, bHasState ? GetQTEStateColor(QTEState) : FLinearColor(0.035f, 0.035f, 0.04f, 0.72f));
	PaintOutline(AllottedGeometry, OutDrawElements, CurrentLayer++, RowPosition, RowSize, bMatched || !bHasState ? FLinearColor(0.20f, 0.34f, 0.48f, 0.9f) : FLinearColor(0.95f, 0.16f, 0.12f, 1.0f), 2.0f * LocalScale);
	PaintText(AllottedGeometry, OutDrawElements, CurrentLayer++, LabelPosition, Label, GetLabelFont(LocalScale), bHasState ? FLinearColor::White : FLinearColor(0.45f, 0.48f, 0.54f, 1.0f));

	const int32 CellCount = FMath::Max(CellContents.Num(), CellStates.Num());
	for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
	{
		const FVector2D CellPosition = CellStart + FVector2D((CellSize.X + CellGap) * CellIndex, 0.0f);
		const ECommonQTECellState CellState = CellStates.IsValidIndex(CellIndex) ? CellStates[CellIndex] : ECommonQTECellState::OnGoing;
		const int32 CellContent = CellContents.IsValidIndex(CellIndex) ? CellContents[CellIndex] : 0;
		PaintBox(AllottedGeometry, OutDrawElements, CurrentLayer++, CellPosition, CellSize, GetCellStateColor(CellState));
		PaintOutline(AllottedGeometry, OutDrawElements, CurrentLayer++, CellPosition, CellSize, CellIndex == FocusCellIndex ? FLinearColor(1.0f, 0.92f, 0.15f, 1.0f) : FLinearColor(0.72f, 0.78f, 0.86f, 0.75f), CellIndex == FocusCellIndex ? 5.0f * LocalScale : 2.0f * LocalScale);

		const FString CellText = FString::FromInt(CellContent);
		const FVector2D TextOffset = FVector2D(13.0f * CellText.Len(), 20.0f) * LocalScale;
		PaintText(AllottedGeometry, OutDrawElements, CurrentLayer++, CellPosition + CellSize * 0.5f - TextOffset, CellText, GetCellFont(LocalScale), FLinearColor::White);
	}

	return CurrentLayer;
}

void SCommonQTEFlowTestVisualizer::PaintBox(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const
{
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		Color);
}

void SCommonQTEFlowTestVisualizer::PaintOutline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color, float Thickness) const
{
	TArray<FVector2D> Points;
	Points.Add(Position);
	Points.Add(Position + FVector2D(Size.X, 0.0f));
	Points.Add(Position + Size);
	Points.Add(Position + FVector2D(0.0f, Size.Y));
	Points.Add(Position);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Color,
		true,
		Thickness);
}

void SCommonQTEFlowTestVisualizer::PaintText(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color) const
{
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(1.0f, 1.0f), FSlateLayoutTransform(Position)),
		Text,
		Font,
		ESlateDrawEffect::None,
		Color);
}
