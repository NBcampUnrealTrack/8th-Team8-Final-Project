#include "InGame/HUD/SFSkillCooldownWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SSFSkillCooldown : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFSkillCooldown) {} SLATE_END_ARGS()
    void Construct(const FArguments&) { SetCanTick(false); }
    void SetCooldown(float InRemaining, float InDuration)
    {
        Remaining = InRemaining;
        Duration = InDuration;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        if (Remaining <= 0.f || Duration <= 0.f) return Layer;
        const FVector2f Size(Geometry.GetLocalSize());
        if (Size.X <= 0.f || Size.Y <= 0.f) return Layer;
        const FVector2f Center = Size * 0.5f;
        const float Start = 2.f * PI * (1.f - FMath::Clamp(Remaining / Duration, 0.f, 1.f));
        auto Boundary = [&](float Angle)
        {
            const FVector2f Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
            return Center + Direction / FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y)) * Center;
        };
        TArray<FVector2f> Points = {Center, Boundary(Start)};
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const float Angle = PI * (0.25f + 0.5f * Corner);
            if (Angle > Start) Points.Add(Boundary(Angle));
        }
        Points.Add(Boundary(2.f * PI));
        const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
        const auto* Proxy = Resource.GetResourceProxy();
        const FVector2f UV = Proxy ? Proxy->StartUV + Proxy->SizeUV * 0.5f : FVector2f(0.5f, 0.5f);
        TArray<FSlateVertex> Vertices;
        TArray<SlateIndex> Indices;
        for (const FVector2f& Point : Points)
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                Geometry.GetAccumulatedRenderTransform(), Point, UV, FColor(0, 0, 0, 190)));
        for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
            Indices.Append({0, static_cast<SlateIndex>(Index), static_cast<SlateIndex>(Index + 1)});
        FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);

        const FString Text = FString::FromInt(FMath::CeilToInt(Remaining));
        const auto Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(10, FMath::RoundToInt(Size.Y * 0.29f)));
        const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
        const FVector2D Position((Size.X - TextSize.X) * 0.5f, Size.Y - TextSize.Y - 2.f);
        FSlateDrawElement::MakeBox(Elements, Layer + 1,
            Geometry.ToPaintGeometry(TextSize + FVector2D(8.f, 0.f), FSlateLayoutTransform(Position - FVector2D(4.f, 0.f))),
            Brush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.65f));
        FSlateDrawElement::MakeText(Elements, Layer + 2,
            Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position)), Text, Font,
            ESlateDrawEffect::None, FLinearColor::White);
        return Layer + 2;
    }
private:
    float Remaining = 0.f;
    float Duration = 30.f;
};

void USFSkillCooldownWidget::SetCooldown(float Remaining, float Duration)
{
    RemainingSeconds = FMath::Max(0.f, Remaining);
    DurationSeconds = Duration;
    if (CooldownSlate) CooldownSlate->SetCooldown(RemainingSeconds, DurationSeconds);
}

TSharedRef<SWidget> USFSkillCooldownWidget::RebuildWidget()
{
    CooldownSlate = SNew(SSFSkillCooldown);
    CooldownSlate->SetCooldown(RemainingSeconds, DurationSeconds);
    return CooldownSlate.ToSharedRef();
}

void USFSkillCooldownWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    CooldownSlate.Reset();
}
