#include "SFResultPresentation.h"
#include "SFEtherVictoryPreview.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SSFResultPresentation : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFResultPresentation) {} SLATE_END_ARGS()
    void Construct(const FArguments&, USFResultPresentation* Widget)
    {
        Owner=Widget; SetCanTick(false); ForceVolatile(true);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const auto* W=Owner.Get(); if (!W) return Layer;
        const FVector2D S=G.GetLocalSize(), C(S.X*.5,S.Y*.42);
        const float Fit=FMath::Min(S.X/1920.,S.Y/1080.);
        const float T=FMath::Max(0.,FSlateApplication::Get().GetCurrentTime()-W->StartTime);
        if (W->bExitButtonDecoration)
            return PaintResultExitButton(G,Out,Layer,T,Style.GetColorAndOpacityTint(),W->bVictory,
                W->ExitButton.IsValid() && (W->ExitButton->IsHovered() || W->ExitButton->HasKeyboardFocus()),
                W->ExitButton.IsValid() && W->ExitButton->IsPressed());
        if (W->bEtherBurstPreview)
            return W->bForeground?Layer:PaintEtherVictoryPreview(G,Out,Layer,T,W->PreviewFont,Style.GetColorAndOpacityTint(),W->bVictory);
        const float Fade=FMath::Clamp(T/.65f,0.f,1.f);
        const FLinearColor Accent=W->bVictory?FLinearColor(1.f,.68f,.22f):FLinearColor(.82f,.19f,.12f);
        const FLinearColor Tint=Style.GetColorAndOpacityTint();
        auto Line=[&](TArray<FVector2D> Points,FLinearColor Color,float Width=1.f)
        {
            FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color*Tint,true,Width*Fit);
        };
        const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
        const auto* Proxy=Resource.GetResourceProxy();
        const FVector2f UV=Proxy?Proxy->StartUV+Proxy->SizeUV*.5f:FVector2f(.5,.5);
        auto Fan=[&](FVector2D Center,float Radius,float Angle,float Spread,FLinearColor Inner,FLinearColor Outer,int Steps)
        {
            TArray<FSlateVertex> V; TArray<SlateIndex> Indices;
            auto Vertex=[&](FVector2D P,FLinearColor Color)
            { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),UV,(Color*Tint).ToFColor(true)); };
            V.Add(Vertex(Center,Inner));
            for (int I=0;I<=Steps;++I)
            {
                const float A=Angle+Spread*I/Steps;
                V.Add(Vertex(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,Outer));
                if (I>0) Indices.Append({0,static_cast<SlateIndex>(I),static_cast<SlateIndex>(I+1)});
            }
            FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,V,Indices,nullptr,0,0);
        };
        auto Spark=[&](FVector2D P,float R,FLinearColor Color)
        {
            Fan(P,R*3,0,2*PI,FLinearColor(Color.R,Color.G,Color.B,Color.A*.35f),FLinearColor::Transparent,16);
            Line({P-FVector2D(R,0),P+FVector2D(R,0)},Color,1.6f);
            Line({P-FVector2D(0,R*1.65),P+FVector2D(0,R*1.65)},Color,1.6f);
            Line({P-FVector2D(R*.4,R*.4),P+FVector2D(R*.4,R*.4)},Color,1.f);
        };
        auto Beam=[&](float Angle,float Spread,float Radius,float Alpha)
        {
            // Fade across both angular edges and along the ray, avoiding hard triangular wedges.
            constexpr int Radial=8, Angular=8;
            TArray<FSlateVertex> V; TArray<SlateIndex> Indices;
            for(int R=0;R<=Radial;++R)
                for(int A=0;A<=Angular;++A)
                {
                    const float Along=R/static_cast<float>(Radial), Across=A/static_cast<float>(Angular);
                    const float Theta=Angle+Spread*(Across-.5f);
                    const FVector2D Pos=C+FVector2D(FMath::Cos(Theta),FMath::Sin(Theta))*(80+Along*Radius)*Fit;
                    FLinearColor Color=Accent;
                    Color.A=Alpha*FMath::Square(FMath::Sin(PI*Across))*FMath::Square(1-Along);
                    V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Pos),UV,(Color*Tint).ToFColor(true)));
                    if(R<Radial && A<Angular)
                    {
                        const SlateIndex I=R*(Angular+1)+A, J=I+Angular+1;
                        Indices.Append({I,static_cast<SlateIndex>(I+1),J,static_cast<SlateIndex>(I+1),static_cast<SlateIndex>(J+1),J});
                    }
                }
            FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,V,Indices,nullptr,0,0);
        };
        if (W->bForeground)
        {
            // Glints sit on the crest jewels, rather than drifting across the lettering.
            const FVector2D Jewels[]={{0,-250},{-320,15},{320,15},{0,175},{-164,-90},{172,-93}};
            const float P=FMath::Clamp(T/.8f,0.f,1.f), Ease=1-FMath::Pow(1-P,3.f);
            const float Scale=.76f+.24f*Ease+.025f*FMath::Sin(P*PI);
            for(int I=0;I<UE_ARRAY_COUNT(Jewels);++I)
            {
                const float Pulse=FMath::Pow(FMath::Max(0.f,FMath::Sin(T*1.7f-I*1.45f)),8.f);
                const FVector2D Pos=C+(Jewels[I]*Scale+FVector2D(0,(1-Ease)*45))*Fit;
                Spark(Pos,(4+Pulse*14)*Fit,FLinearColor(1,.95f,.75f,(.15f+Pulse*.8f)*Fade));
            }
            // An outward burst accompanies the impact, then leaves only occasional jewel glints.
            const float Burst=FMath::Clamp((T-.15f)/2.2f,0.f,1.f);
            if (Burst>0 && Burst<1)
            {
                for(int I=0;I<(W->bVictory?54:24);++I)
                {
                    const float Seed=FMath::Frac(I*.61803399f), A=I*2.39996f;
                    const FVector2D Direction(FMath::Cos(A),FMath::Sin(A)*.7);
                    const FVector2D Pos=C+(Direction*(230+(360+Seed*280)*FMath::Sqrt(Burst))+FVector2D(0,100*Burst*Burst))*Fit;
                    const FLinearColor Color=I%3==0 && W->bVictory?FLinearColor(.35f,.9f,1,(1-Burst)*.9f):FLinearColor(Accent.R,Accent.G,Accent.B,(1-Burst)*.9f);
                    Line({Pos-Direction*(5+Seed*14)*Fit,Pos},Color,2.f);
                    if(I%5==0) Spark(Pos,5*Fit,Color);
                }
            }
            return Layer;
        }
        // The field remains recognisable, but all visual attention moves to the crest.
        FSlateDrawElement::MakeBox(Out,Layer++,G.ToPaintGeometry(),FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,FLinearColor(.004f,.009f,.018f,.82f*Fade)*Tint);
        Fan(C,760*Fit,0,2*PI,FLinearColor(Accent.R,Accent.G,Accent.B,.5f*Fade),FLinearColor(0,0,0,0),80);
        Fan(C+FVector2D(0,-120)*Fit,440*Fit,0,2*PI,W->bVictory?FLinearColor(.08f,.5f,1,.32f*Fade):FLinearColor(.8f,.08f,.03f,.2f*Fade),FLinearColor::Transparent,64);
        ++Layer;
        const int Rays=W->bVictory?24:14;
        for(int I=0;I<Rays;++I)
        {
            const float A=2*PI*I/Rays+T*(W->bVictory?.018f:-.009f);
            const float Pulse=.8f+.2f*FMath::Sin(T*.7f+I);
            Beam(A,I%2?.075f:.16f,I%2?850:1050,.45f*Fade*Pulse);
        }
        ++Layer;
        // Expanding impact rings establish the arrival before settling into the halo.
        for(int Ring=0;Ring<2;++Ring)
        {
            const float P=FMath::Clamp((T-.12f-Ring*.18f)/1.1f,0.f,1.f);
            if(P<=0 || P>=1) continue;
            TArray<FVector2D> Points;
            for(int I=0;I<=96;++I)
            {
                const float A=2*PI*I/96;
                Points.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A)*.75f)*(200+550*P)*Fit);
            }
            Line(Points,FLinearColor(Accent.R,Accent.G,Accent.B,(1-P)*.6f),3.f*(1-P)+1);
        }
        // Interrupted concentric rings frame the crest.
        for(int Ring=0;Ring<2;++Ring)
        {
            for(int Arc=0;Arc<12;++Arc)
            {
                TArray<FVector2D> Points;
                for(int I=0;I<=8;++I)
                {
                    const float A=2*PI*(Arc/12.f+I/8.f*.063f)+T*.007f*(Ring?1:-1);
                    Points.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*(370+Ring*18)*Fit);
                }
                Line(Points,FLinearColor(Accent.R,Accent.G,Accent.B,.13f*Fade));
            }
        }
        ++Layer;
        for(int I=0;I<(W->bVictory?110:45);++I)
        {
            const float Seed=FMath::Frac(I*.61803399f);
            const float X=FMath::Frac(I*.75487766f)*S.X;
            const float Y=(1.f-FMath::Frac(Seed+T*(.018f+Seed*.025f)))*S.Y;
            const float Twinkle=FMath::Square(.5f+.5f*FMath::Sin(T*(1.f+Seed)+I));
            const float Alpha=(.12f+Twinkle*.65f)*Fade*FMath::Sin(PI*Y/S.Y);
            const FVector2D P(X,Y); const float R=(1.f+Seed*2.5f)*Fit;
            const FLinearColor Color(Accent.R,Accent.G,Accent.B,Alpha);
            if(I%4==0) Spark(P,R*2,Color);
            else
            {
                // Tiny gold / sapphire diamond confetti catches the rotating light.
                const float Side=R*(.3f+.7f*FMath::Abs(FMath::Sin(T+I)));
                Line({P-FVector2D(0,R*1.4),P+FVector2D(Side,0),P+FVector2D(0,R*1.4),P-FVector2D(Side,0),P-FVector2D(0,R*1.4)},
                    W->bVictory && I%3==0?FLinearColor(.3f,.8f,1,Alpha):Color,1.3f);
            }
        }
        // A quiet inset gold border closes the composition without covering HUD-sized margins.
        const float Inset=32*Fit;
        Line({{Inset,Inset},{S.X-Inset,Inset}},FLinearColor(Accent.R,Accent.G,Accent.B,.25f*Fade));
        Line({{Inset,S.Y-Inset},{S.X-Inset,S.Y-Inset}},FLinearColor(Accent.R,Accent.G,Accent.B,.25f*Fade));
        return Layer;
    }
private:
    TWeakObjectPtr<USFResultPresentation> Owner;
};

TSharedRef<SWidget> USFResultPresentation::RebuildWidget()
{
    return SNew(SSFResultPresentation,this);
}
