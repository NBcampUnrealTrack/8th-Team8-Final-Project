#include "InGame/HUD/Tutorial/SFTutorialSpotlightWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

void USFTutorialSpotlightWidget::ApplyGuideStep(
    const FSFStageIntroGuideStep& InStep,
    int32 InStepIndex,
    int32 InStepCount
)
{
    CurrentStep = InStep;
    bHasStep = true;
    bTargetVisible = true;
    PulseTime = 0.f;

    // 화면 좌표 칸은 값이 고정이라 여기서 한 번만 계산한다.
    // 월드 좌표 칸은 카메라가 움직이므로 NativeTick 이 매 프레임 다시 잡는다.
    if (CurrentStep.TargetKind == ESFGuideTargetKind::ScreenRegion)
    {
        HoleCenterNorm = CurrentStep.ScreenAnchor;
        HoleRadiusNorm = CurrentStep.ScreenRadius;
    }

    if (IsValid(InstructionText))
    {
        InstructionText->SetText(CurrentStep.InstructionText);
        InstructionText->SetVisibility(
            CurrentStep.InstructionText.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::HitTestInvisible);
    }

    if (IsValid(StepCounterText))
    {
        // 한 칸짜리 안내에 "1 / 1" 을 띄우면 오히려 눈에 거슬린다.
        if (InStepCount <= 1)
        {
            StepCounterText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            StepCounterText->SetText(
                FText::Format(
                    NSLOCTEXT("SFTutorialSpotlight", "StepCounterFormat", "{0} / {1}"),
                    FText::AsNumber(InStepIndex + 1),
                    FText::AsNumber(InStepCount)));

            StepCounterText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }

    // 입력을 막을지는 칸마다 다르다.
    //
    // 클릭으로 넘기는 칸은 클릭을 받아야 하므로 덮개가 입력을 가져간다.
    // 그 외에는 플레이어가 실제로 조작해야 한다 — 여기서 막으면
    // "드래그하세요" 안내가 드래그 자체를 막는다.
    SetVisibility(
        CurrentStep.Completion == ESFGuideCompletion::Click
        ? ESlateVisibility::Visible
        : ESlateVisibility::SelfHitTestInvisible);

    OnGuideStepApplied(CurrentStep);
}

void USFTutorialSpotlightWidget::ClearGuide()
{
    bHasStep = false;

    if (IsValid(InstructionText))
    {
        InstructionText->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(StepCounterText))
    {
        StepCounterText->SetVisibility(ESlateVisibility::Collapsed);
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

    UpdateInstructionPosition(MyGeometry);
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

void USFTutorialSpotlightWidget::UpdateInstructionPosition(const FGeometry& MyGeometry)
{
    if (!IsValid(InstructionText) || !bInstructionFollowsSpotlight)
    {
        return;
    }

    // 캔버스에 놓지 않았다면 레이아웃을 손대지 않는다.
    // 화면 아래에 고정해 두고 쓰는 것도 흔한 구성이라 강제하지 않는다.
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(InstructionText->Slot);

    if (CanvasSlot == nullptr)
    {
        return;
    }

    const FVector2D LocalSize = MyGeometry.GetLocalSize();

    if (LocalSize.Y <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float RadiusPx = static_cast<float>(HoleRadiusNorm * LocalSize.Y);

    FVector2D Position = HoleCenterNorm * LocalSize;

    // 구멍 아래에 붙인다. 아래가 화면 밖으로 나가면 위로 넘긴다.
    const float Margin = RadiusPx + FMath::Max(InstructionOffset, 0.f);
    Position.Y = (Position.Y + Margin > LocalSize.Y * 0.9f)
        ? Position.Y - Margin
        : Position.Y + Margin;

    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetPosition(Position);
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

    const FSlateBrush* Brush = DimBrush.GetResourceObject() != nullptr
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
    const float RadiusPx = static_cast<float>(HoleRadiusNorm * Size.Y);
    const FVector2D Center = HoleCenterNorm * Size;

    const float Left = FMath::Clamp<float>(Center.X - RadiusPx, 0.f, Size.X);
    const float Right = FMath::Clamp<float>(Center.X + RadiusPx, 0.f, Size.X);
    const float Top = FMath::Clamp<float>(Center.Y - RadiusPx, 0.f, Size.Y);
    const float Bottom = FMath::Clamp<float>(Center.Y + RadiusPx, 0.f, Size.Y);

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

    // 테두리. 덮개를 끈 칸에서도 그린다 — 이때는 이것만으로 자리를 알린다.
    const float Pulse = PulseSpeed > 0.f
        ? (FMath::Sin(PulseTime * PulseSpeed * 2.f * PI) * 0.5f + 0.5f)
        : 1.f;

    const float PulseScale = 1.f - PulseAmount + PulseAmount * Pulse;

    FLinearColor RingColor = HighlightColor;
    RingColor.A *= PulseScale;

    const int32 RingLayer = DimLayer + 1;
    const float Thickness = FMath::Max(1.f, HighlightThickness * PulseScale);
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
