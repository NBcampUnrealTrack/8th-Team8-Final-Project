#include "SFDefenceTimerWidget.h"
#include "Game/Defence/SFDefenceWaveController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void USFDefenceTimerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    TimerBorder = WidgetTree->ConstructWidget<UBorder>();
    TimerBorder->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.92f));
    TimerBorder->SetPadding(FMargin(24.f, 12.f));
    UCanvasPanelSlot* TimerSlot = Canvas->AddChildToCanvas(TimerBorder);
    TimerSlot->SetAnchors(FAnchors(0.5f, 0.f));
    TimerSlot->SetAlignment(FVector2D(0.5f, 0.f));
    TimerSlot->SetPosition(FVector2D(0.f, 24.f));
    TimerSlot->SetSize(FVector2D(300.f, 108.f));
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
    TimerBorder->SetContent(Box);
    PhaseText = WidgetTree->ConstructWidget<UTextBlock>();
    TimeText = WidgetTree->ConstructWidget<UTextBlock>();
    for (UTextBlock* Text : {PhaseText.Get(), TimeText.Get()})
    {
        Text->SetJustification(ETextJustify::Center);
        Text->SetShadowOffset(FVector2D(1.f, 1.f));
        Text->SetShadowColorAndOpacity(FLinearColor::Black);
        Box->AddChildToVerticalBox(Text);
    }
    FSlateFontInfo LabelFont = PhaseText->GetFont();
    LabelFont.Size = 19;
    PhaseText->SetFont(LabelFont);
    FSlateFontInfo ClockFont = TimeText->GetFont();
    ClockFont.Size = 36;
    TimeText->SetFont(ClockFont);
}

void USFDefenceTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!IsValid(Controller) || !PhaseText || !TimeText) return;
    const bool bPreparing = Controller->GetPhase() == ESFDefencePhase::Preparation;
    PhaseText->SetText(FText::Format(bPreparing
        ? NSLOCTEXT("SFDefence", "PreparationOfTotal", "웨이브 {0}/{1} 준비시간")
        : NSLOCTEXT("SFDefence", "WaveOfTotal", "웨이브 {0}/{1} · 방어 중"),
        FText::AsNumber(Controller->GetWaveNumber() + (bPreparing ? 1 : 0)),
        FText::AsNumber(FMath::Max(1, Controller->TotalWaves))));
    const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Controller->GetRemainingSeconds()));
    TimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
    const FLinearColor Accent = bPreparing ? FLinearColor(0.35f, 0.85f, 1.f)
        : FLinearColor(1.f, 0.48f, 0.24f);
    PhaseText->SetColorAndOpacity(Accent);
    TimeText->SetColorAndOpacity(Seconds <= 10 ? FLinearColor(1.f, 0.25f, 0.2f) : FLinearColor::White);
}
