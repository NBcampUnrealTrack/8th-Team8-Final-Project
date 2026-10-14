#include "InGame/HUD/Tutorial/SFStageIntroPortraitFrame.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SLeafWidget.h"

namespace SFPortraitGeometry
{
    // 첨부된 2:3 휘장의 테두리 아래로 조금 겹치는 내부 다각형.
    // 초상화와 배경을 같은 모양으로 잘라 아래쪽 사선 밖으로 새지 않게 한다.
    TArray<FVector2f> MakeShield(const FVector2f& Size)
    {
        TArray<FVector2f> Points = {
            {0.14f, 0.075f}, {0.86f, 0.075f}, {0.89f, 0.115f},
            {0.89f, 0.80f}, {0.78f, 0.86f}, {0.50f, 0.915f},
            {0.22f, 0.86f}, {0.11f, 0.80f}, {0.11f, 0.115f}
        };
        for (FVector2f& Point : Points)
        {
            Point *= Size;
        }
        return Points;
    }

    // 텍스처 바깥 UV가 반복되지 않도록 휘장과 초상화 사각형의 교집합만 그린다.
    TArray<FVector2f> ClipToRect(TArray<FVector2f> Points, const FVector2f& Min, const FVector2f& Max)
    {
        for (int32 Edge = 0; Edge < 4 && !Points.IsEmpty(); ++Edge)
        {
            auto Distance = [&](const FVector2f& P)
            {
                switch (Edge)
                {
                case 0: return P.X - Min.X;
                case 1: return Max.X - P.X;
                case 2: return P.Y - Min.Y;
                default: return Max.Y - P.Y;
                }
            };
            TArray<FVector2f> Result;
            FVector2f Previous = Points.Last();
            float PreviousDistance = Distance(Previous);
            for (const FVector2f& Point : Points)
            {
                const float CurrentDistance = Distance(Point);
                if ((PreviousDistance >= 0.f) != (CurrentDistance >= 0.f))
                {
                    Result.Add(FMath::Lerp(Previous, Point,
                        PreviousDistance / (PreviousDistance - CurrentDistance)));
                }
                if (CurrentDistance >= 0.f)
                {
                    Result.Add(Point);
                }
                Previous = Point;
                PreviousDistance = CurrentDistance;
            }
            Points = MoveTemp(Result);
        }
        return Points;
    }
}

/** 휘장은 고정하고 초상화만 확대/보정하며 화자 강조 색을 실시간으로 읽는다. */
class SSFFramedPortrait : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFFramedPortrait) {}
        SLATE_ARGUMENT(UImage*, Portrait)
        SLATE_ARGUMENT(const FSlateBrush*, Frame)
        SLATE_ARGUMENT(FVector2D, Size)
        SLATE_ARGUMENT(FMargin, Inset)
        SLATE_ARGUMENT(float, Zoom)
        SLATE_ARGUMENT(FVector2D, Offset)
        SLATE_ARGUMENT(FLinearColor, Background)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Portrait = InArgs._Portrait;
        Frame = InArgs._Frame;
        Size = FVector2f(InArgs._Size);
        Inset = InArgs._Inset;
        Zoom = InArgs._Zoom;
        Offset = FVector2f(InArgs._Offset);
        Background = InArgs._Background;
        // UImage는 UMG 바인딩용으로 유지되며, 그 색을 읽는 이 Slate 위젯이 다시 그려져야 한다.
        ForceVolatile(true);
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size); }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        UImage* Image = Portrait.Get();
        // 이 Image는 데이터/화자 강조용이며 Slate 트리에 직접 올리지 않는다.
        // IsVisible()은 캐시된 Slate가 없으면 항상 false이므로 휘장까지 사라진다.
        if (!IsValid(Image) || Image->GetVisibility() == ESlateVisibility::Hidden
            || Image->GetVisibility() == ESlateVisibility::Collapsed)
        {
            return Layer;
        }
        const FVector2f Available(Geometry.GetLocalSize());
        if (Available.X <= 0.f || Available.Y <= 0.f)
        {
            return Layer;
        }
        // WBP 가 잡아둔 칸이 휘장 비율과 달라도 비율을 지킨 채 아래쪽에 맞춰 넣는다.
        // 칸을 무시하고 고정 크기로 그리면 칸 밖으로 나가 아무것도 안 보인다.
        const FVector2f LocalSize = (Size.X > 0.f && Size.Y > 0.f)
            ? Size * FMath::Min(Available.X / Size.X, Available.Y / Size.Y)
            : Available;
        const FVector2f Origin((Available.X - LocalSize.X) * 0.5f, Available.Y - LocalSize.Y);
        TArray<FVector2f> Shield = SFPortraitGeometry::MakeShield(LocalSize);
        for (FVector2f& Point : Shield)
        {
            Point += Origin;
        }
        const FLinearColor StyleTint = Style.GetColorAndOpacityTint();

        auto DrawPolygon = [&](const TArray<FVector2f>& Points, const FSlateBrush& Brush,
            int32 DrawLayer, const FVector2f& ImagePosition, const FVector2f& ImageSize,
            FLinearColor Color, bool bGradient)
        {
            if (Points.Num() < 3) return;
            const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush);
            const FSlateShaderResourceProxy* Proxy = Resource.GetResourceProxy();
            TArray<FSlateVertex> Vertices;
            TArray<SlateIndex> Indices;
            for (const FVector2f& Point : Points)
            {
                FVector2f UV = (Point - ImagePosition) / ImageSize;
                if (Proxy) UV = Proxy->StartUV + UV * Proxy->SizeUV;
                FLinearColor VertexColor = Color;
                if (bGradient)
                {
                    const float Shade = FMath::Lerp(1.2f, 0.55f, (Point.Y - Origin.Y) / LocalSize.Y);
                    VertexColor.R *= Shade;
                    VertexColor.G *= Shade;
                    VertexColor.B *= Shade;
                }
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                    Geometry.GetAccumulatedRenderTransform(), Point, UV, (VertexColor * StyleTint).ToFColor(true)));
            }
            for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
            {
                Indices.Append({0, static_cast<SlateIndex>(Index), static_cast<SlateIndex>(Index + 1)});
            }
            FSlateDrawElement::MakeCustomVerts(Elements, DrawLayer, Resource, Vertices, Indices, nullptr, 0, 0);
        };

        DrawPolygon(Shield, *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), Layer,
            FVector2f::ZeroVector, LocalSize, Background, true);

        const UTexture2D* Texture = Cast<UTexture2D>(Image->GetBrush().GetResourceObject());
        if (IsValid(Texture) && Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0)
        {
            const FVector2f NaturalSize(Texture->GetSizeX(), Texture->GetSizeY());
            const FVector2f AreaSize = LocalSize * FVector2f(1.f - Inset.Left - Inset.Right, 1.f - Inset.Top - Inset.Bottom);
            const float Fit = FMath::Min(AreaSize.X / NaturalSize.X, AreaSize.Y / NaturalSize.Y);
            // 귀가 잘리지 않도록 위쪽 기준으로 확대한다. 몸통은 방패 모양에 맞춰 잘린다.
            const FVector2f DrawSize = NaturalSize * Fit * Zoom * FVector2f(Image->GetRenderTransform().Scale);
            const FVector2f Position = Origin + FVector2f(
                LocalSize.X * Inset.Left + (AreaSize.X - DrawSize.X) * 0.5f + Offset.X * LocalSize.X,
                LocalSize.Y * Inset.Top + Offset.Y * LocalSize.Y);
            FLinearColor Tint = Image->GetColorAndOpacity() * Image->GetBrush().GetTint(Style);
            Tint.A *= Image->GetRenderOpacity();
            if (DrawSize.X > 0.f && DrawSize.Y > 0.f)
            {
                DrawPolygon(SFPortraitGeometry::ClipToRect(Shield, Position, Position + DrawSize),
                    Image->GetBrush(), Layer + 1, Position, DrawSize, Tint, false);
            }
        }
        FSlateDrawElement::MakeBox(Elements, Layer + 2,
            Geometry.ToPaintGeometry(FVector2D(LocalSize.X, LocalSize.Y),
                FSlateLayoutTransform(FVector2D(Origin.X, Origin.Y))),
            Frame, ESlateDrawEffect::None, Frame->GetTint(Style) * StyleTint);
        return Layer + 2;
    }

private:
    TWeakObjectPtr<UImage> Portrait;
    const FSlateBrush* Frame = nullptr;
    FVector2f Size = FVector2f(320.f, 480.f);
    FMargin Inset;
    float Zoom = 1.f;
    FVector2f Offset;
    FLinearColor Background;
};

void USFStageIntroPortraitFrame::SetFrame(
    UTexture2D* Texture, const FVector2D& Size, const FMargin& Inset,
    float Zoom, const FVector2D& Offset, const FLinearColor& BackgroundColor)
{
    FrameBrush.SetResourceObject(Texture);
    FrameBrush.DrawAs = ESlateBrushDrawType::Image;
    FrameSize = FVector2D(FMath::Max(1.0, Size.X), FMath::Max(1.0, Size.Y));
    // 각 변을 45% 이하로 제한해 초상화 영역이 뒤집히지 않게 한다.
    FrameInset = FMargin(
        FMath::Clamp(Inset.Left, 0.f, 0.45f), FMath::Clamp(Inset.Top, 0.f, 0.45f),
        FMath::Clamp(Inset.Right, 0.f, 0.45f), FMath::Clamp(Inset.Bottom, 0.f, 0.45f));
    PortraitZoom = FMath::Clamp(Zoom, 0.5f, 3.f);
    PortraitOffset = Offset;
    BackdropColor = BackgroundColor;
    RefreshPresentation();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPortraitUncachedVisibilityTest,
    "StrikeForge.UI.PortraitFrame.UncachedImageVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPortraitUncachedVisibilityTest::RunTest(const FString& Parameters)
{
    // 실제 오류와 동일하게 UImage::TakeWidget()을 호출하지 않은 상태에서 그린다.
    UImage* SourceImage = NewObject<UImage>();
    SourceImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    TestFalse(TEXT("Source image has no cached Slate widget"), SourceImage->GetCachedWidget().IsValid());
    const FSlateBrush* TestBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    TSharedRef<SSFFramedPortrait> PortraitWidget = SNew(SSFFramedPortrait)
        .Portrait(SourceImage).Frame(TestBrush).Size(FVector2D(320.f, 480.f))
        .Inset(FMargin(0.17f, 0.13f, 0.17f, 0.23f)).Zoom(1.6f)
        .Offset(FVector2D::ZeroVector).Background(FLinearColor::Black);
    TSharedRef<SWindow> TestWindow = SNew(SWindow).ClientSize(FVector2D(320.f, 480.f));
    FHittestGrid HitTestGrid;
    FPaintArgs PaintArgs(nullptr, HitTestGrid, FVector2D::ZeroVector, 0.0, 0.f);
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(320.f, 480.f), FSlateLayoutTransform());
    const FSlateRect Bounds(0.f, 0.f, 320.f, 480.f);
    auto Paint = [&]()
    {
        FSlateWindowElementList Elements(TestWindow);
        return PortraitWidget->OnPaint(PaintArgs, Geometry, Bounds, Elements, 0, FWidgetStyle(), true);
    };
    TestEqual(TEXT("Uncached visible image draws the frame"), Paint(), 2);
    SourceImage->SetVisibility(ESlateVisibility::Hidden);
    TestEqual(TEXT("Hidden image suppresses the frame"), Paint(), 0);
    SourceImage->SetVisibility(ESlateVisibility::Collapsed);
    TestEqual(TEXT("Collapsed image suppresses the frame"), Paint(), 0);
    SourceImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    TestEqual(TEXT("Returning to dialogue restores the frame"), Paint(), 2);
    return true;
}
#endif

TSharedRef<SWidget> USFStageIntroPortraitFrame::RebuildWidget()
{
    SAssignNew(Container, SBox);
    RefreshPresentation();
    return Container.ToSharedRef();
}

void USFStageIntroPortraitFrame::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    RefreshPresentation();
}

void USFStageIntroPortraitFrame::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Container.Reset();
}

void USFStageIntroPortraitFrame::OnSlotAdded(UPanelSlot* InSlot)
{
    RefreshPresentation();
}

void USFStageIntroPortraitFrame::OnSlotRemoved(UPanelSlot* InSlot)
{
    if (Container.IsValid())
    {
        Container->SetContent(SNullWidget::NullWidget);
    }
}

void USFStageIntroPortraitFrame::RefreshPresentation()
{
    if (!Container.IsValid())
    {
        return;
    }

    UImage* Portrait = Cast<UImage>(GetContent());
    // 이전 Slate 부모를 먼저 해제한다. UMG의 Image와 기존 바인딩은 유지한다.
    Container->SetContent(SNullWidget::NullWidget);
    if (!IsValid(Portrait))
    {
        return;
    }

    if (!IsValid(FrameBrush.GetResourceObject()))
    {
        // None으로 바꾸면 원래 슬롯 크기와 Image 브러시 크기를 그대로 사용한다.
        Container->SetHAlign(HAlign_Fill);
        Container->SetVAlign(VAlign_Fill);
        Container->SetContent(Portrait->TakeWidget());
        return;
    }

    // 원래 Image 가 쓰던 칸을 그대로 채운다. 여기서 크기를 강제하면
    // 캔버스 슬롯이 그보다 작거나 0 일 때 휘장과 초상화가 함께 사라진다.
    Container->SetHAlign(HAlign_Fill);
    Container->SetVAlign(VAlign_Fill);
    Container->SetContent(
        SNew(SSFFramedPortrait)
            .Portrait(Portrait).Frame(&FrameBrush).Size(FrameSize).Inset(FrameInset)
            .Zoom(PortraitZoom).Offset(PortraitOffset).Background(BackdropColor));
}
