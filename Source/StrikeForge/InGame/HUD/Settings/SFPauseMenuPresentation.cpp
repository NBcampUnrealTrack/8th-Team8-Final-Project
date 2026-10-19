#include "SFPauseMenuPresentation.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SSFPauseMenuPresentation : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFPauseMenuPresentation) {} SLATE_END_ARGS()
    void Construct(const FArguments&, USFPauseMenuPresentation* InOwner)
    {
        Owner = InOwner;
        SetCanTick(false);
        ForceVolatile(true);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const auto* W = Owner.Get();
        if (!W) return Layer;
        const FVector2D Size = G.GetLocalSize();
        const float Time = FMath::Max(0., FSlateApplication::Get().GetCurrentTime() - W->StartTime);
        const FLinearColor Tint = Style.GetColorAndOpacityTint();
        const auto* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
        const auto* Proxy = Resource.GetResourceProxy();
        const FVector2f UV = Proxy ? Proxy->StartUV + Proxy->SizeUV * .5f : FVector2f(.5f, .5f);
        const FLinearColor Gold(.78f, .52f, .23f), Cream(1.f, .89f, .61f), Cyan(.1f, .83f, .88f);
        bool bFrameGeometry = true;
        auto MapPoint = [&](FVector2D P)
        {
            if (W->bSettingsPanel && !W->bBackdrop)
            {
                P.X = 480 + (P.X - 310) * (bFrameGeometry ? 1.5 : 1.);
                if (bFrameGeometry) P.Y = 130 + (P.Y - 130) * 1.318;
            }
            return P;
        };
        auto Vertex = [&](FVector2D P, FLinearColor Color)
        {
            return FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),
                FVector2f(MapPoint(P)), UV, (Color * Tint).ToFColor(true));
        };
        auto Polygon = [&](const TArray<FVector2D>& Points, FLinearColor Top, FLinearColor Bottom)
        {
            if (Points.Num() < 3) return;
            double MinY = Points[0].Y, MaxY = MinY;
            for (const auto& P : Points) { MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y); }
            TArray<FSlateVertex> V;
            TArray<SlateIndex> Indices;
            for (const auto& P : Points)
                V.Add(Vertex(P, FMath::Lerp(Top, Bottom, static_cast<float>((P.Y - MinY) / FMath::Max(1., MaxY - MinY)))));
            for (int32 I = 1; I + 1 < V.Num(); ++I)
                Indices.Append({0, static_cast<SlateIndex>(I), static_cast<SlateIndex>(I + 1)});
            FSlateDrawElement::MakeCustomVerts(Out, Layer++, Resource, V, Indices, nullptr, 0, 0);
        };
        auto Line = [&](TArray<FVector2D> Points, FLinearColor Color, float Width = 1.f)
        {
            for (auto& P : Points) P = MapPoint(P);
            FSlateDrawElement::MakeLines(Out, Layer++, G.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, Color * Tint, true, Width);
        };
        auto Glow = [&](FVector2D Center, float Radius, FLinearColor Color, float Aspect = 1.f)
        {
            TArray<FSlateVertex> V;
            TArray<SlateIndex> Indices;
            V.Add(Vertex(Center, Color));
            for (int32 I = 0; I <= 48; ++I)
            {
                const float Angle = 2 * PI * I / 48;
                V.Add(Vertex(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle) * Aspect) * Radius,
                    FLinearColor::Transparent));
                if (I) Indices.Append({0, static_cast<SlateIndex>(I), static_cast<SlateIndex>(I + 1)});
            }
            FSlateDrawElement::MakeCustomVerts(Out, Layer++, Resource, V, Indices, nullptr, 0, 0);
        };
        if (W->bBackdrop)
        {
            FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(), White,
                ESlateDrawEffect::None, FLinearColor(.003f, .009f, .014f, .82f) * Tint);
            const float Fit = FMath::Min(Size.X / 1920., Size.Y / 1080.);
            Glow(Size * .5, 760 * Fit, FLinearColor(.015f, .23f, .25f, .23f), .8f);
            for (int32 I = 0; I < 24; ++I)
            {
                const float Seed = FMath::Frac(I * .618034f);
                const FVector2D P(FMath::Frac(I * .754878f + .13f) * Size.X,
                    (1 - FMath::Frac(Seed + Time * (.008f + Seed * .008f))) * Size.Y);
                const float A = (.15f + .2f * FMath::Square(FMath::Sin(Time + I))) * FMath::Sin(PI * P.Y / FMath::Max(1., Size.Y));
                Glow(P, (3 + Seed * 4) * Fit, FLinearColor(.95f, .7f, .3f, A));
            }
            return Layer;
        }

        // Authored at 620 x 740, fitted together with the real controls by a ScaleBox.
        auto Frame = [](double X, double Y, double Width, double Height, double Cut)
        {
            return TArray<FVector2D>{{X + Cut, Y}, {X + Width - Cut, Y}, {X + Width, Y + Cut},
                {X + Width, Y + Height - Cut}, {X + Width - Cut, Y + Height}, {X + Cut, Y + Height},
                {X, Y + Height - Cut}, {X, Y + Cut}};
        };
        Glow({310, 345}, 365, FLinearColor(0, 0, 0, .75f), 1.12f);
        Polygon(Frame(28, 130, 564, 566, 34), FLinearColor(.04f, .025f, .014f), FLinearColor(.008f, .012f, .015f));
        Polygon(Frame(32, 134, 556, 558, 32), Cream, FLinearColor(.27f, .13f, .045f));
        Polygon(Frame(40, 142, 540, 542, 29), FLinearColor(.18f, .12f, .055f), Gold);
        Polygon(Frame(44, 146, 532, 534, 27), FLinearColor(.018f, .068f, .074f), FLinearColor(.004f, .016f, .025f));
        auto Inset = Frame(52, 154, 516, 518, 25);
        const FVector2D FirstCorner = Inset[0];
        Inset.Add(FirstCorner);
        Line(Inset, FLinearColor(.58f, .43f, .21f, .5f));
        Glow({310, 185}, 260, FLinearColor(.02f, .48f, .47f, .12f), .75f);
        // Corner plates are deliberately broad, echoing the lobby's ivory / gold bevels.
        for (int Side : {-1, 1})
        {
            const auto P = [Side](double X, double Y) { return FVector2D(310 + Side * X, Y); };
            Polygon({P(225, 134), P(247, 134), P(278, 165), P(278, 188), P(264, 182), P(231, 148)},
                FLinearColor(.94f, .84f, .61f), FLinearColor(.45f, .3f, .13f));
            Polygon({P(278, 638), P(278, 661), P(247, 692), P(225, 692), P(231, 678), P(264, 644)},
                FLinearColor(.59f, .43f, .22f), FLinearColor(.27f, .17f, .08f));
            Line({P(226, 162), P(246, 183), P(246, 207)}, FLinearColor(.76f, .61f, .34f, .65f), 1.5f);
            Line({P(226, 665), P(246, 645), P(246, 621)}, FLinearColor(.76f, .61f, .34f, .5f), 1.5f);
        }

        bFrameGeometry = false;
        const FVector2D Crest(310, 128);
        Glow(Crest, 145, FLinearColor(.06f, .65f, .69f, .18f + .05f * FMath::Sin(Time * 1.6f)));
        for (int32 Ring = 0; Ring < 2; ++Ring)
        {
            TArray<FVector2D> Arc;
            for (int32 I = 0; I <= 72; ++I)
            {
                const float A = 2 * PI * I / 72;
                Arc.Add(Crest + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (66 + Ring * 9));
            }
            Line(Arc, FLinearColor(.67f, .51f, .25f, Ring ? .22f : .55f));
        }
        for (int Side : {-1, 1})
        {
            const auto P = [Side](double X, double Y) { return FVector2D(310 + Side * X, 128 + Y); };
            for (int Feather = 0; Feather < 3; ++Feather)
            {
                const double X = 56 + Feather * 24;
                Polygon({P(32 + Feather * 12, 16), P(X, -12), P(X + 39, -42 + Feather * 13),
                    P(X + 20, 1 + Feather * 11), P(46 + Feather * 13, 29)},
                    FLinearColor(.95f, .83f, .55f), FLinearColor(.33f, .19f, .065f));
                Line({P(X, -12), P(X + 39, -42 + Feather * 13)}, Cream, 1.3f);
            }
        }
        Polygon({{310, 50}, {367, 127}, {310, 199}, {253, 127}}, Cream, FLinearColor(.41f, .23f, .075f));
        Polygon({{310, 64}, {354, 127}, {310, 183}, {266, 127}}, FLinearColor(.11f, .24f, .22f), FLinearColor(.015f, .04f, .05f));
        Polygon({{310, 73}, {346, 127}, {310, 174}, {274, 127}}, FLinearColor(.15f, .93f, .91f), FLinearColor(.015f, .23f, .31f));
        Polygon({{310, 73}, {310, 126}, {274, 127}}, FLinearColor(.65f, 1.f, .91f), Cyan);
        Polygon({{310, 126}, {346, 127}, {310, 174}}, FLinearColor(.025f, .57f, .67f), FLinearColor(.005f, .12f, .21f));
        Line({{274, 127}, {310, 73}, {346, 127}}, FLinearColor(.8f, 1.f, .94f), 1.7f);
        const float Glint = .4f + .35f * FMath::Sin(Time * 1.7f);
        Glow({310, 83}, 20, FLinearColor(.6f, 1.f, .92f, Glint));

        // Dividers use the same small diamond as the crest and the result buttons.
        if (!W->bSettingsPanel) for (double Y : {316., 638.})
        {
            Line({{114, Y}, {288, Y}}, FLinearColor(.64f, .47f, .24f, .45f));
            Line({{332, Y}, {506, Y}}, FLinearColor(.64f, .47f, .24f, .45f));
            Polygon({{310, Y - 5}, {316, Y}, {310, Y + 5}, {304, Y}}, Gold, FLinearColor(.3f, .2f, .08f));
        }
        return Layer;
    }
private:
    TWeakObjectPtr<USFPauseMenuPresentation> Owner;
};

TSharedRef<SWidget> USFPauseMenuPresentation::RebuildWidget()
{
    return SNew(SSFPauseMenuPresentation, this);
}
