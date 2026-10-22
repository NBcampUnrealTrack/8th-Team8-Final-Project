#include "SFOptionsPresentation.h"
#include "Components/Slider.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SSFOptionsPresentation : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFOptionsPresentation) {} SLATE_END_ARGS()
    void Construct(const FArguments&, USFOptionsPresentation* InOwner)
    { Owner = InOwner; SetCanTick(false); ForceVolatile(true); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const auto* W = Owner.Get();
        if (!W) return Layer;
        using namespace SFOptionsColors;
        const double Now = FSlateApplication::Get().GetCurrentTime();
        const float Time = FMath::Max(0., Now - W->StartTime);
        const float Delta = FMath::Clamp(Now - LastPaint, 0., .05);
        LastPaint = Now;
        const FLinearColor Tint = Style.GetColorAndOpacityTint();
        const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
        const auto* Proxy = Resource.GetResourceProxy();
        const FVector2f UV = Proxy ? Proxy->StartUV + Proxy->SizeUV * .5f : FVector2f(.5f, .5f);
        const auto Gold = Hex(TEXT("D5AF68")), Ivory = Hex(TEXT("FFF0CB")), Aqua = Hex(TEXT("36E3CE"));
        auto Vertex = [&](FVector2D P, FLinearColor C)
        { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), UV, (C * Tint).ToFColor(true)); };
        auto Polygon = [&](const TArray<FVector2D>& Points, FLinearColor Top, FLinearColor Bottom)
        {
            double MinY = Points[0].Y, MaxY = MinY;
            for (const auto& P : Points) { MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y); }
            TArray<FSlateVertex> V; TArray<SlateIndex> Indices;
            for (const auto& P : Points) V.Add(Vertex(P, FMath::Lerp(Top, Bottom, float((P.Y - MinY) / FMath::Max(1., MaxY - MinY)))));
            for (int I = 1; I + 1 < V.Num(); ++I) Indices.Append({0, SlateIndex(I), SlateIndex(I + 1)});
            FSlateDrawElement::MakeCustomVerts(Out, Layer++, Resource, V, Indices, nullptr, 0, 0);
        };
        auto Line = [&](TArray<FVector2D> Points, FLinearColor C, float Width = 1.f)
        { FSlateDrawElement::MakeLines(Out, Layer++, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C * Tint, true, Width); };
        auto Glow = [&](FVector2D Center, float Radius, FLinearColor C, float Aspect = 1.f)
        {
            TArray<FSlateVertex> V; TArray<SlateIndex> Indices; V.Add(Vertex(Center, C));
            for (int I = 0; I <= 40; ++I)
            {
                const float A = 2 * PI * I / 40;
                V.Add(Vertex(Center + FVector2D(FMath::Cos(A), FMath::Sin(A) * Aspect) * Radius, FLinearColor::Transparent));
                if (I) Indices.Append({0, SlateIndex(I), SlateIndex(I + 1)});
            }
            FSlateDrawElement::MakeCustomVerts(Out, Layer++, Resource, V, Indices, nullptr, 0, 0);
        };
        auto Frame = [](double X, double Y, double Width, double Height, double Cut)
        { return TArray<FVector2D>{{X+Cut,Y},{X+Width-Cut,Y},{X+Width,Y+Cut},{X+Width,Y+Height-Cut},{X+Width-Cut,Y+Height},{X+Cut,Y+Height},{X,Y+Height-Cut},{X,Y+Cut}}; };
        auto Outline = [&](TArray<FVector2D> Points, FLinearColor C, float Width = 1.f)
        { const auto First = Points[0]; Points.Add(First); Line(Points, C, Width); };
        auto Gem = [&](FVector2D P, float R)
        {
            Polygon({P+FVector2D(0,-R*1.4),P+FVector2D(R,0),P+FVector2D(0,R*1.4),P+FVector2D(-R,0)}, Ivory, Gold);
            R *= .76f;
            Polygon({P+FVector2D(0,-R*1.4),P+FVector2D(R,0),P+FVector2D(0,R*1.4),P+FVector2D(-R,0)}, Hex(TEXT("9FFFF0")), Hex(TEXT("007D79")));
            Polygon({P+FVector2D(0,-R*1.4),P,P+FVector2D(-R,0)}, Hex(TEXT("C6FFF1")), Aqua);
            Polygon({P,P+FVector2D(R,0),P+FVector2D(0,R*1.4)}, Hex(TEXT("16C7B9")), Hex(TEXT("00544E")));
        };
        Glow({620,450}, 650, FLinearColor(0,0,0,.72f), .7f);
        Polygon(Frame(24,119,1192,737,38), Hex(TEXT("33291D")), Hex(TEXT("090F0E")));
        Polygon(Frame(28,123,1184,729,35), Ivory, Hex(TEXT("936E37")));
        Polygon(Frame(37,132,1166,711,31), Gold, Hex(TEXT("685438")));
        Polygon(Frame(42,137,1156,701,28), Hex(TEXT("07514A")), Hex(TEXT("032B29")));
        Outline(Frame(51,146,1138,683,24), Hex(TEXT("7EA185")).CopyWithNewOpacity(.5f));
        Glow({620,179}, 460, Aqua.CopyWithNewOpacity(.13f), .46f);
        // Subtle engraved rays stay behind the header, away from the controls.
        for (int Side : {-1,1})
        {
            auto P = [Side](double X, double Y) { return FVector2D(620 + Side * X,Y); };
            for (int I = 0; I < 4; ++I)
                Line({P(130+I*20,157),P(245+I*55,242),P(470+I*20,242)}, Gold.CopyWithNewOpacity(.08f + I*.015f));
            Polygon({P(535,123),P(557,123),P(592,158),P(592,185),P(575,174),P(541,142)}, Ivory, Gold);
            Polygon({P(592,794),P(592,817),P(557,852),P(535,852),P(547,834),P(575,806)}, Gold, Hex(TEXT("876236")));
            Gem(P(583,487), 12);
            Line({P(563,190),P(563,443)}, Gold.CopyWithNewOpacity(.28f));
            Line({P(563,530),P(563,785)}, Gold.CopyWithNewOpacity(.28f));
        }
        // Header seal: faceted gold wings and a breathing mint crystal.
        Glow({620,122}, 130, Aqua.CopyWithNewOpacity(.17f + .05f*FMath::Sin(Time*1.6f)));
        for (int Side : {-1,1})
            for (int I = 0; I < 3; ++I)
            {
                auto P = [Side](double X,double Y){return FVector2D(620+Side*X,122+Y);};
                Polygon({P(24+I*13,20),P(50+I*24,-8),P(91+I*23,-35+I*12),P(73+I*20,15),P(38+I*18,32)}, Ivory, Hex(TEXT("AC8444")));
                Line({P(50+I*24,-8),P(91+I*23,-35+I*12)}, Ivory, 1.4f);
            }
        Gem({620,122}, 42);
        const float Spark = .5f + .3f*FMath::Sin(Time*1.9f);
        Glow({620,88}, 20, Ivory.CopyWithNewOpacity(Spark));
        Line({{614,88},{626,88}}, Ivory.CopyWithNewOpacity(Spark), 1.2f);
        Line({{620,82},{620,94}}, Ivory.CopyWithNewOpacity(Spark), 1.2f);

        for (double X : {82.,644.})
        {
            Polygon(Frame(X,292,514,424,16), Hex(TEXT("216C60")), Hex(TEXT("165448")));
            Polygon(Frame(X+1,293,512,422,15), Hex(TEXT("073D36")), Hex(TEXT("052E2B")));
            Polygon(Frame(X+2,294,510,82,14), Hex(TEXT("0A5B50")), Hex(TEXT("08473F")));
            Line({{X+22,376},{X+492,376}}, Gold.CopyWithNewOpacity(.45f));
            Gem({X+487,327}, 5);
        }
        // Screen and speaker pictograms are drawn as vectors, independent of fonts.
        Outline(Frame(104,309,44,35,4), Ivory, 2.f);
        Line({{126,344},{126,352},{114,352},{138,352}}, Ivory, 2.f);
        Polygon({{670,322},{680,322},{693,311},{693,346},{680,336},{670,336}}, Ivory, Gold);
        for (int I = 0; I < 2; ++I)
        {
            TArray<FVector2D> Arc;
            for (int J = 0; J <= 18; ++J)
            { const float A = -.85f + J*1.7f/18; Arc.Add(FVector2D(694,329)+FVector2D(FMath::Cos(A),FMath::Sin(A))*(9+I*8)); }
            Line(Arc, Aqua.CopyWithNewOpacity(.8f), 2.f);
        }
        // Smooth hover / keyboard focus outlines. No layout movement on hover.
        const FVector4 Rows[] = {FVector4(100,392,478,104),FVector4(100,494,478,104),FVector4(100,596,478,108),
            FVector4(662,392,478,104),FVector4(662,494,478,104),FVector4(662,596,478,108)};
        for (int I = 0; I < 6; ++I)
        {
            const auto* Control = W->Controls.IsValidIndex(I) ? W->Controls[I].Get() : nullptr;
            const bool bActive = Control && (Control->IsHovered() || Control->HasKeyboardFocus());
            Hover[I] = FMath::FInterpTo(Hover[I], bActive ? 1.f : 0.f, Delta, 12.f);
            const auto R = Rows[I];
            if (Hover[I] > .005f)
            {
                Polygon(Frame(R.X,R.Y,R.Z,R.W,8), Aqua.CopyWithNewOpacity(.045f*Hover[I]), Aqua.CopyWithNewOpacity(.012f*Hover[I]));
                Outline(Frame(R.X,R.Y,R.Z,R.W,8), Aqua.CopyWithNewOpacity(.42f*Hover[I]));
                Line({{R.X,R.Y+18},{R.X,R.Y+R.W-18}}, Aqua.CopyWithNewOpacity(.9f*Hover[I]), 2.f);
            }
        }
        const FVector2D Tracks[] = {{118,653},{680,449},{680,551},{680,653}};
        for (int I = 0; I < 4; ++I)
        {
            const auto* Slider = W->Sliders.IsValidIndex(I) ? W->Sliders[I].Get() : nullptr;
            if (!Slider) continue;
            const float Value = Slider->GetNormalizedValue();
            if (!FMath::IsNearlyEqual(LastValues[I], Value, .0001f))
            { LastValues[I] = Value; ChangedAt[I] = Now; }
            const float Pulse = FMath::Clamp(1. - (Now-ChangedAt[I])*2.5, 0., 1.);
            const float H = Hover[I == 0 ? 2 : I+2];
            const FVector2D P = Tracks[I], End = P + FVector2D(440,0), Thumb = P + FVector2D(440*Value,0);
            Line({P,End}, Hex(TEXT("021C1A")), 8.f);
            Line({P+FVector2D(0,4),End+FVector2D(0,4)}, Hex(TEXT("54836B")).CopyWithNewOpacity(.4f));
            if (Value > .002f) Line({P,Thumb}, Aqua, 5.f);
            for (int Tick = 0; Tick <= 10; ++Tick)
            {
                const FVector2D T = P+FVector2D(Tick*44,11);
                Line({T,T+FVector2D(0,Tick%5 == 0 ? 5 : 2)}, Gold.CopyWithNewOpacity(Tick/10.f <= Value ? .7f : .22f));
            }
            Glow(Thumb, 22 + Pulse*12, Aqua.CopyWithNewOpacity(.12f+.18f*H+.12f*Pulse));
            Gem(Thumb, 8.5f + H*1.5f);
        }
        // A quiet traveling glint along the gold top edge, never across the text.
        const float T = FMath::Fmod(Time*.055f,1.f);
        const FVector2D Glint(100+1040*T,134);
        Glow(Glint,24,Ivory.CopyWithNewOpacity(.35f*FMath::Sin(PI*T)),.3f);
        for (int I=0; I<9; ++I)
        {
            const float Seed=FMath::Frac(I*.618034f);
            const FVector2D P(70+FMath::Frac(I*.754878f)*1100, 164+FMath::Frac(Seed-Time*.015f)*95);
            Glow(P,2+Seed*2,Gold.CopyWithNewOpacity(.12f+.1f*FMath::Sin(Time+I)));
        }
        Line({{91,741},{1149,741}}, Gold.CopyWithNewOpacity(.28f));
        Line({{105,767},{111,773},{122,760}}, Aqua, 2.f);
        return Layer;
    }
private:
    TWeakObjectPtr<USFOptionsPresentation> Owner;
    mutable double LastPaint=0.;
    mutable float Hover[6]={};
    mutable float LastValues[4]={-1,-1,-1,-1};
    mutable double ChangedAt[4]={};
};

TSharedRef<SWidget> USFOptionsPresentation::RebuildWidget()
{ return SNew(SSFOptionsPresentation,this); }
