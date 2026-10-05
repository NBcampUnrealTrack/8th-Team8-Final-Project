#include "InGame/HUD/Tutorial/SFTutorialSpotlightWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

void USFTutorialSpotlightWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ForceVolatile(true);

    // 우측 상단 안내 배너는 두 줄 문구가 자주 들어간다. 기존 크기에서는
    // 외곽선까지 겹쳐 보이므로 폰트만 조금 낮추고 줄 간격을 확보한다.
    if (IsValid(InstructionText))
    {
        FSlateFontInfo GuideFont = InstructionText->GetFont();
        GuideFont.Size = FMath::Max(1, GuideFont.Size - 4);
        InstructionText->SetFont(GuideFont);
        InstructionText->SetLineHeightPercentage(1.18f);
    }
}

void USFTutorialSpotlightWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void USFTutorialSpotlightWidget::ApplyGuideStep(
    const FSFStageIntroGuideStep& InStep,
    int32 /*InStepIndex*/,
    int32 /*InStepCount*/
)
{
    CurrentStep = InStep;
    bHasStep = true;
    bTargetVisible = USFTutorialSubsystem::GetProductionGuideUnit(CurrentStep.CompletionTag).IsNone();
    PulseTime = 0.f;

    // 월드 좌표 칸은 카메라 거리로 크기를 정하므로 축별 지정을 쓰지 않는다.
    HoleExtentNorm = FVector2D::ZeroVector;

    // 화면 좌표 칸은 값이 고정이라 여기서 한 번만 계산한다.
    // 월드 좌표 칸은 카메라가 움직이므로 NativeTick 이 매 프레임 다시 잡는다.
    if (CurrentStep.TargetKind == ESFGuideTargetKind::ScreenRegion)
    {
        HoleCenterNorm = CurrentStep.ScreenAnchor;
        HoleRadiusNorm = CurrentStep.ScreenRadius;
        HoleExtentNorm = CurrentStep.ScreenExtent;
    }

    if (IsValid(InstructionText))
    {
        InstructionText->SetText(CurrentStep.InstructionText);
        InstructionText->SetVisibility(
            CurrentStep.InstructionText.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::HitTestInvisible);
    }

    // 입력을 막을지는 칸마다 다르다.
    //
    // 클릭으로 넘기는 칸은 클릭을 받아야 하므로 덮개가 입력을 가져간다.
    // 그 외에는 플레이어가 실제로 조작해야 한다 — 여기서 막으면
    // "드래그하세요" 안내가 드래그 자체를 막는다.
    //
    // SelfHitTestInvisible 이 아니라 HitTestInvisible 이어야 한다.
    // 앞의 것은 "나는 안 받지만 자식은 받는다" 라서, 화면을 덮는 캔버스
    // 패널이 그대로 클릭을 먹는다. 위젯을 만든 사람이 캔버스 가시성을
    // 어떻게 뒀는지에 따라 되기도 하고 안 되기도 하는 상태가 된다.
    SetVisibility(
        CurrentStep.Completion == ESFGuideCompletion::Click
        ? ESlateVisibility::Visible
        : ESlateVisibility::HitTestInvisible);

    OnGuideStepApplied(CurrentStep);
    UpdateSkillGuideCard();
}

void USFTutorialSpotlightWidget::UpdateSkillGuideCard()
{
    if (!WidgetTree || !InstructionText) return;
    UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!Root) return;
    const bool bSkillGuide = CurrentStep.CommanderSkillTier > 0;
    if (!bSkillGuide)
    {
        if (SkillGuideCard) SkillGuideCard->SetVisibility(ESlateVisibility::Collapsed);
        for (UBorder* Edge : SkillHighlightEdges) Edge->SetVisibility(ESlateVisibility::Collapsed);
        for (const auto& Entry : OriginalGuideVisibility)
            if (Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
        OriginalGuideVisibility.Reset();
        return;
    }
    if (!SkillGuideCard)
    {
        SkillGuideCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CommanderSkillGuideCard"));
        SkillGuideCard->SetBrushColor(FLinearColor(1.f, 0.78f, 0.12f));
        SkillGuideCard->SetPadding(FMargin(3.f));
        USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetWidthOverride(520.f);
        SkillGuideCard->SetContent(Width);
        UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
        Backdrop->SetBrushColor(FLinearColor(0.018f, 0.022f, 0.03f, 0.96f));
        Backdrop->SetPadding(FMargin(18.f, 14.f));
        Width->SetContent(Backdrop);
        SkillGuideText = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font = InstructionText->GetFont();
        Font.Size = 28;
        SkillGuideText->SetFont(Font);
        SkillGuideText->SetColorAndOpacity(FLinearColor::White);
        SkillGuideText->SetWrapTextAt(484.f);
        SkillGuideText->SetLineHeightPercentage(1.18f);
        Backdrop->SetContent(SkillGuideText);
        UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(SkillGuideCard);
        CardSlot->SetAnchors(FAnchors(1.f, 0.f));
        CardSlot->SetAlignment(FVector2D(1.f, 0.f));
        CardSlot->SetPosition(FVector2D(-24.f, 24.f));
        CardSlot->SetAutoSize(true);
        CardSlot->SetZOrder(10);
    }
    for (UWidget* Child : Root->GetAllChildren())
    {
        if (Child == SkillGuideCard || SkillHighlightEdges.Contains(Cast<UBorder>(Child))) continue;
        if (!OriginalGuideVisibility.Contains(Child)) OriginalGuideVisibility.Add(Child, Child->GetVisibility());
        Child->SetVisibility(ESlateVisibility::Collapsed);
    }
    SkillGuideCard->SetVisibility(ESlateVisibility::HitTestInvisible);
    SkillGuideText->SetText(CurrentStep.InstructionText);
    if (SkillHighlightEdges.IsEmpty())
    {
        for (int32 Index = 0; Index < 4; ++Index)
        {
            UBorder* Edge = WidgetTree->ConstructWidget<UBorder>();
            Edge->SetBrushColor(FLinearColor(1.f, 0.9f, 0.f));
            Edge->SetPadding(FMargin(0.f));
            Edge->SetVisibility(ESlateVisibility::HitTestInvisible);
            Root->AddChildToCanvas(Edge)->SetZOrder(100);
            SkillHighlightEdges.Add(Edge);
        }
    }
}

void USFTutorialSpotlightWidget::ClearGuide()
{
    bHasStep = false;

    if (IsValid(InstructionText))
    {
        InstructionText->SetVisibility(ESlateVisibility::Collapsed);
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void USFTutorialSpotlightWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bHasStep)
    {
        return;
    }

    PulseTime += InDeltaTime;

    if (CurrentStep.TargetKind == ESFGuideTargetKind::ScreenRegion)
    {
        const bool bResolved = UpdateFromScreenTargetWidget(MyGeometry);
        if (CurrentStep.CommanderSkillTier > 0 || !USFTutorialSubsystem::GetProductionGuideUnit(CurrentStep.CompletionTag).IsNone())
            bTargetVisible = bResolved;
        if (CurrentStep.CommanderSkillTier > 0 && SkillHighlightEdges.Num() == 4)
        {
            const FVector2D Center = HoleCenterNorm * MyGeometry.GetLocalSize();
            const FVector2D Extent = HoleExtentNorm * MyGeometry.GetLocalSize();
            const FVector2D TopLeft = Center - Extent;
            const FVector2D Size = Extent * 2.f;
            const float Thickness = 2.f / FMath::Max(0.1f, MyGeometry.GetAccumulatedLayoutTransform().GetScale());
            const FVector2D Positions[] = {TopLeft, TopLeft + FVector2D(0, Size.Y-Thickness), TopLeft, TopLeft + FVector2D(Size.X-Thickness, 0)};
            const FVector2D Sizes[] = {FVector2D(Size.X, Thickness), FVector2D(Size.X, Thickness), FVector2D(Thickness, Size.Y), FVector2D(Thickness, Size.Y)};
            for (int32 Index = 0; Index < 4; ++Index)
            {
                SkillHighlightEdges[Index]->SetVisibility(bResolved ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
                if (auto* EdgeSlot = Cast<UCanvasPanelSlot>(SkillHighlightEdges[Index]->Slot))
                {
                    EdgeSlot->SetPosition(Positions[Index]);
                    EdgeSlot->SetSize(Sizes[Index]);
                }
            }
        }
    }

    if (CurrentStep.TargetKind == ESFGuideTargetKind::WorldLocation)
    {
        FVector2D Center = FVector2D::ZeroVector;
        float Radius = 0.f;

        bTargetVisible = ProjectStepToScreen(Center, Radius);

        if (bTargetVisible)
        {
            HoleCenterNorm = Center;
            HoleRadiusNorm = Radius;
        }
    }

}

UWidget* USFTutorialSpotlightWidget::ResolveScreenTargetWidget() const
{
    const ASFInGamePlayerController* Controller = Cast<ASFInGamePlayerController>(GetOwningPlayer());
    if (!Controller && GetWorld()) Controller = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>();
    USFInGameHUDWidget* HUD = IsValid(Controller) ? Controller->GetInGameHUDWidget() : nullptr;
    if (!IsValid(HUD))
    {
        return nullptr;
    }

    if (CurrentStep.CommanderSkillTier > 0)
    {
        return HUD->GetTutorialCommanderSkillWidget(CurrentStep.CommanderSkillTier);
    }

    static const FGameplayTag BuildModeOpenTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.BuildMode.Open"), false);
    if (CurrentStep.CompletionTag == BuildModeOpenTag)
    {
        return HUD->GetTutorialBuildModeButton();
    }

    const FName ProductionUnit = USFTutorialSubsystem::GetProductionGuideUnit(CurrentStep.CompletionTag);
    if (!ProductionUnit.IsNone())
    {
        return HUD->FindTutorialProductionButton(ProductionUnit);
    }

    static const FGameplayTag SelectBuildingTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Select.Building"), false);
    if (!CurrentStep.RequiredBuildingRowName.IsNone() &&
        SelectBuildingTag.IsValid() &&
        CurrentStep.CompletionTag.MatchesTag(SelectBuildingTag))
    {
        return HUD->FindTutorialBuildCatalogEntry(CurrentStep.RequiredBuildingRowName);
    }

    return nullptr;
}

bool USFTutorialSpotlightWidget::UpdateFromScreenTargetWidget(const FGeometry& OverlayGeometry)
{
    UWidget* Target = ResolveScreenTargetWidget();
    const FVector2D OverlaySize = OverlayGeometry.GetLocalSize();
    if (!IsValid(Target) || Target->GetVisibility() == ESlateVisibility::Collapsed ||
        OverlaySize.X <= KINDA_SMALL_NUMBER || OverlaySize.Y <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FGeometry& TargetGeometry = Target->GetCachedGeometry();
    const FVector2D TargetSize = TargetGeometry.GetLocalSize();
    if (TargetSize.X <= KINDA_SMALL_NUMBER || TargetSize.Y <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D TopLeft = OverlayGeometry.AbsoluteToLocal(TargetGeometry.LocalToAbsolute(FVector2D::ZeroVector));
    const FVector2D BottomRight = OverlayGeometry.AbsoluteToLocal(TargetGeometry.LocalToAbsolute(TargetSize));
    const FVector2D TargetPadding(8.f, 8.f);
    HoleCenterNorm = ((TopLeft + BottomRight) * 0.5f) / OverlaySize;
    HoleExtentNorm = (((BottomRight - TopLeft) * 0.5f) + TargetPadding) / OverlaySize;
    bTargetVisible = true;
    return true;
}

// ─────────────────────────────────────────────────────────
// 월드 좌표 투영
//
// 반지름을 상수로 두지 않는 이유: 카메라가 줌아웃하면 같은 건물이 화면에서
// 작아지는데, 구멍 크기가 그대로면 주변 절반을 함께 밝히게 된다.
// 월드 반지름만큼 옆으로 간 점을 같이 투영해 화면 크기를 역산한다.
// ─────────────────────────────────────────────────────────
bool USFTutorialSpotlightWidget::ProjectStepToScreen(FVector2D& OutCenter, float& OutRadius) const
{
    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return false;
    }

    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(const_cast<USFTutorialSpotlightWidget*>(this));

    if (ViewportSize.X <= KINDA_SMALL_NUMBER || ViewportSize.Y <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FVector2D CenterPixel = FVector2D::ZeroVector;

    // 대상이 카메라 뒤에 있으면 false 가 나온다. 이때 좌표를 그대로 쓰면
    // 화면 반대편에 구멍이 뚫린다.
    if (!UGameplayStatics::ProjectWorldToScreen(PlayerController, CurrentStep.WorldLocation, CenterPixel, false))
    {
        return false;
    }

    OutCenter = CenterPixel / ViewportSize;

    // 카메라 기준 오른쪽으로 WorldRadius 만큼 간 점을 함께 투영한다.
    FVector CameraLocation = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;
    PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
    const FVector EdgeLocation = CurrentStep.WorldLocation + CameraRight * CurrentStep.WorldRadius;

    FVector2D EdgePixel = FVector2D::ZeroVector;

    if (UGameplayStatics::ProjectWorldToScreen(PlayerController, EdgeLocation, EdgePixel, false))
    {
        OutRadius = static_cast<float>(FMath::Abs(EdgePixel.X - CenterPixel.X) / ViewportSize.Y);
    }
    else
    {
        OutRadius = 0.12f;
    }

    // 너무 작으면 안 보이고, 너무 크면 덮개가 없는 것과 같아진다.
    OutRadius = FMath::Clamp(OutRadius, 0.04f, 0.45f);

    return true;
}

// ─────────────────────────────────────────────────────────
// 구멍 크기
//
// ScreenExtent 를 지정했으면 축별 비율을 그대로 쓰고, 아니면 예전처럼
// 세로 기준 정사각형을 만든다. 기존 DT 값은 손대지 않아도 그대로 동작한다.
// ─────────────────────────────────────────────────────────
FVector2D USFTutorialSpotlightWidget::GetHoleExtentPx(const FVector2D& InSize) const
{
    if (HoleExtentNorm.X > 0.f && HoleExtentNorm.Y > 0.f)
    {
        return HoleExtentNorm * InSize;
    }

    const float RadiusPx = static_cast<float>(HoleRadiusNorm * InSize.Y);

    return FVector2D(RadiusPx, RadiusPx);
}

// ─────────────────────────────────────────────────────────
// 그리기
//
// 구멍 바깥을 사각형 네 장으로 덮는다. 위 / 아래 / 왼쪽 / 오른쪽 순서이며
// 좌우 두 장은 위아래와 겹치지 않도록 세로 범위를 구멍 높이로 줄인다.
// 겹치면 그 부분만 두 번 칠해져 더 어둡게 보인다.
// ─────────────────────────────────────────────────────────
int32 USFTutorialSpotlightWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    // Skill guides use real UMG borders so the square stays visible in cached HUDs.
    if (CurrentStep.CommanderSkillTier > 0) return LayerId;
    const int32 BaseLayer = Super::NativePaint(
        Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (!bHasStep)
    {
        return BaseLayer;
    }

    const FVector2D Size = AllottedGeometry.GetLocalSize();

    if (Size.X <= KINDA_SMALL_NUMBER || Size.Y <= KINDA_SMALL_NUMBER)
    {
        return BaseLayer;
    }

    const FSlateBrush* Brush = CurrentStep.CommanderSkillTier == 0 && DimBrush.GetResourceObject() != nullptr
        ? &DimBrush
        : FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

    if (Brush == nullptr)
    {
        return BaseLayer;
    }

    const int32 DimLayer = BaseLayer + 1;

    // 대상이 카메라 뒤에 있으면 구멍 없이 전체를 덮는다.
    // 엉뚱한 자리를 밝히는 것보다 낫다.
    const bool bDrawHole = bTargetVisible;

    // FVector2D 가 배정밀도라 명시적으로 float 로 받는다.
    // 섞어 쓰면 FMath::Clamp 템플릿 추론이 갈린다.
    const FVector2D ExtentPx = GetHoleExtentPx(Size);
    const FVector2D Center = HoleCenterNorm * Size;

    const float HalfX = static_cast<float>(ExtentPx.X);
    const float HalfY = static_cast<float>(ExtentPx.Y);

    const float Left = FMath::Clamp<float>(Center.X - HalfX, 0.f, Size.X);
    const float Right = FMath::Clamp<float>(Center.X + HalfX, 0.f, Size.X);
    const float Top = FMath::Clamp<float>(Center.Y - HalfY, 0.f, Size.Y);
    const float Bottom = FMath::Clamp<float>(Center.Y + HalfY, 0.f, Size.Y);

    // 사각형 한 장을 그리는 헬퍼. 덮개와 테두리가 같은 방식을 쓴다.
    //
    // 선(MakeLines) 대신 얇은 사각형으로 테두리를 그리는 이유는 단순하다 —
    // 그리는 방법이 하나면 엔진 버전에 따라 한쪽만 깨지는 일이 없다.
    auto DrawQuad = [&](int32 InLayer, const FVector2D& InPosition, const FVector2D& InSize, const FLinearColor& InColor)
        {
            if (InSize.X <= 0.f || InSize.Y <= 0.f)
            {
                return;
            }

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                InLayer,
                AllottedGeometry.ToPaintGeometry(InSize, FSlateLayoutTransform(InPosition)),
                Brush,
                ESlateDrawEffect::None,
                InColor);
        };

    if (CurrentStep.bDimBackground)
    {
        if (!bDrawHole)
        {
            DrawQuad(DimLayer, FVector2D::ZeroVector, Size, DimColor);
        }
        else
        {
            // 위 / 아래는 화면 폭 전체, 좌 / 우는 구멍 높이만큼만 그린다.
            // 겹치면 그 부분만 두 번 칠해져 더 어둡게 보인다.
            DrawQuad(DimLayer, FVector2D(0.f, 0.f), FVector2D(Size.X, Top), DimColor);
            DrawQuad(DimLayer, FVector2D(0.f, Bottom), FVector2D(Size.X, Size.Y - Bottom), DimColor);
            DrawQuad(DimLayer, FVector2D(0.f, Top), FVector2D(Left, Bottom - Top), DimColor);
            DrawQuad(DimLayer, FVector2D(Right, Top), FVector2D(Size.X - Right, Bottom - Top), DimColor);
        }
    }

    if (!bDrawHole)
    {
        return DimLayer + 1;
    }

    // 단계별로 강조 테두리를 끌 수 있다. 문구는 계속 표시하되,
    // 특정 단계에서만 가운데 사각형을 숨기고 싶을 때 사용한다.
    if (!CurrentStep.bShowHighlight)
    {
        return DimLayer + 1;
    }

    // 테두리. 덮개를 끈 칸에서도 그린다 — 이때는 이것만으로 자리를 알린다.
    const float Pulse = PulseSpeed > 0.f
        ? (FMath::Sin(PulseTime * PulseSpeed * 2.f * PI) * 0.5f + 0.5f)
        : 1.f;

    const float PulseScale = 1.f - PulseAmount + PulseAmount * Pulse;

    FLinearColor RingColor = CurrentStep.CommanderSkillTier > 0 ? FLinearColor(1.f, 0.9f, 0.f) : HighlightColor;
    RingColor.A *= PulseScale;

    const int32 RingLayer = DimLayer + 1;
    const float MinimumThickness = CurrentStep.CommanderSkillTier > 0
        ? 2.f / FMath::Max(0.1f, AllottedGeometry.GetAccumulatedLayoutTransform().GetScale()) : 1.f;
    const float Thickness = FMath::Max(MinimumThickness, HighlightThickness * PulseScale);
    const float Width = Right - Left;
    const float Height = Bottom - Top;

    DrawQuad(RingLayer, FVector2D(Left, Top), FVector2D(Width, Thickness), RingColor);
    DrawQuad(RingLayer, FVector2D(Left, Bottom - Thickness), FVector2D(Width, Thickness), RingColor);
    DrawQuad(RingLayer, FVector2D(Left, Top), FVector2D(Thickness, Height), RingColor);
    DrawQuad(RingLayer, FVector2D(Right - Thickness, Top), FVector2D(Thickness, Height), RingColor);

    return RingLayer + 1;
}

FReply USFTutorialSpotlightWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent
)
{
    // Click 칸에서만 Visible 이므로, 여기까지 들어왔다는 것 자체가
    // "클릭으로 넘기는 칸" 이라는 뜻이다.
    if (bHasStep && CurrentStep.Completion == ESFGuideCompletion::Click)
    {
        if (USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this))
        {
            Tutorial->CompleteCurrentGuideStep();
        }

        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
