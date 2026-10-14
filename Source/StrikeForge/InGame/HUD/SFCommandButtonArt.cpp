#include "InGame/HUD/SFCommandButtonArt.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/DrawElements.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
static TAutoConsoleVariable<int32> CVarCommandPreview(TEXT("SF.HUD.ButtonPreview"),0,
    TEXT("Editor art preview: 0 actual input, 1 hover, 2 pressed."));
#endif

class SSFCommandButtonArt : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFCommandButtonArt) {} SLATE_END_ARGS()
    void Construct(const FArguments&,USFCommandButtonArt* InOwner)
    {
        Owner=InOwner;SetCanTick(true);ForceVolatile(true);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(80,80); }
    virtual void Tick(const FGeometry& G,double Time,float Delta) override
    {
        const auto* W=Owner.Get();const auto* B=W?W->OwnerButton.Get():nullptr;
        bool Hover=B && (B->IsHovered() || B->HasKeyboardFocus()),Press=B && B->IsPressed();
#if WITH_EDITOR
        Hover|=CVarCommandPreview.GetValueOnGameThread()>0;
        Press|=CVarCommandPreview.GetValueOnGameThread()==2;
#endif
        if(Press && !bWasPressed)LastPressTime=Time;
        bWasPressed=Press;
        HoverAmount=FMath::FInterpTo(HoverAmount,Hover?1.f:0.f,Delta,14.f);
        PressAmount=FMath::FInterpTo(PressAmount,Press?1.f:0.f,Delta,24.f);
        SLeafWidget::Tick(G,Time,Delta);
    }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,
        FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
    {
        const auto* W=Owner.Get();if (!W) return Layer;
        const FVector2D Scale=G.GetLocalSize()/FVector2D(80,80);
        const float Lift=-2.f*HoverAmount+3.f*PressAmount;
        const FLinearColor Tint=Style.GetColorAndOpacityTint()*(Enabled?1.f:.4f);
        const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
        const auto* Proxy=Resource.GetResourceProxy();
        const FVector2f UV=Proxy?Proxy->StartUV+Proxy->SizeUV*.5f:FVector2f(.5,.5);
        auto Poly=[&](TArray<FVector2D> Points,FLinearColor Color)
        {
            TArray<FSlateVertex> V;TArray<SlateIndex> I;
            for (auto P:Points) V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f((P+FVector2D(0,Lift))*Scale),UV,(Color*Tint).ToFColor(true)));
            for(int32 N=1;N+1<V.Num();++N)I.Append({0,static_cast<SlateIndex>(N),static_cast<SlateIndex>(N+1)});
            FSlateDrawElement::MakeCustomVerts(Out,Layer++,Resource,V,I,nullptr,0,0);
        };
        auto Line=[&](TArray<FVector2D> Points,FLinearColor Color,float Width=1.f)
        {
            for(auto& P:Points)P=(P+FVector2D(0,Lift))*Scale;
            FSlateDrawElement::MakeLines(Out,Layer++,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color*Tint,true,Width*FMath::Min(Scale.X,Scale.Y));
        };
        auto Oct=[](float L,float T,float R,float B,float C)
        {return TArray<FVector2D>{{L+C,T},{R-C,T},{R,T+C},{R,B-C},{R-C,B},{L+C,B},{L,B-C},{L,T+C}};};
        // Cast shadow, thick bronze body, bright upper bevel and a recessed slate face.
        Poly(Oct(3,7,77,79,9),FLinearColor(0,0,0,.8));
        Poly(Oct(3,3,77,75,9),FLinearColor(.15,.065,.021,1));
        Poly(Oct(4,3,76,71,8),FLinearColor(.69,.4,.12,1));
        Poly({{12,3},{68,3},{76,11},{72,14},{65,8},{15,8},{8,15},{4,11}},FLinearColor(.95,.73,.38,1));
        Poly(Oct(8,8,72,69,7),FLinearColor(.018,.029,.035,1));
        Poly({{15,10},{65,10},{70,15},{70,34},{10,34},{10,15}},FLinearColor(.065+.025*HoverAmount,.091+.055*HoverAmount,.098+.06*HoverAmount,1));
        Line({{11,56},{11,62},{16,67},{64,67},{69,62}},FLinearColor(.36,.22,.075,1),1.5);
        auto Rim=Oct(8,8,72,69,7);const FVector2D RimStart=Rim[0];Rim.Add(RimStart);
        for(int32 N=3;N>=1;--N)Line(Rim,FLinearColor(.08,.85,.77,HoverAmount*.085),N*3.f);
        Line(Rim,FLinearColor(.18,.9,.81,.22+.72*HoverAmount),1.3);
        const float ClickLight=FMath::Exp(-FMath::Max(0.,FSlateApplication::Get().GetCurrentTime()-LastPressTime)*7.5);
        if(ClickLight>.015f)
        {
            Line(Rim,FLinearColor(1,.77,.36,.62*ClickLight),2.1f);
            Line({{18,11},{34,11}},FLinearColor(.6,1,1,.75*ClickLight),2.f);
            Line({{46,66},{62,66}},FLinearColor(.6,1,1,.75*ClickLight),2.f);
        }
        // Small gold rivets and an ether inlay make the button read as a metal object.
        for(float X:{14.f,66.f})
        {
            Poly({{X,13},{X+2,15},{X,17},{X-2,15}},FLinearColor(.86,.59,.23,1));
            Line({{X-1,14},{X,13},{X+1,14}},FLinearColor(1,.86,.56,1));
        }
        Poly({{34,70},{40,66},{46,70},{40,74}},FLinearColor(.035,.34,.34,1));
        Poly({{36,70},{40,67},{44,70},{40,72}},FLinearColor(.19,.68+.25*HoverAmount,.67+.25*HoverAmount,1));
        const FLinearColor Ivory=FMath::Lerp(FLinearColor(.85,.75,.5,1),FLinearColor(.75,1,.94,1),HoverAmount);
        const FLinearColor Edge(.35,.19,.065,1);
        if(W->Symbol==0)
        {
            // Two distinct bevelled blades with guards and wrapped handles.
            for(int32 S=0;S<2;++S)
            {
                auto P=[&](double X,double Y){return FVector2D(S?80-X:X,Y);};
                Poly({P(25,20),P(34,24),P(53,48),P(48,52),P(28,29)},Ivory);
                Line({P(25,20),P(29,28),P(50,50)},FLinearColor(1,1,.86,.95),1.4);
                Line({P(44,51),P(54,43)},Edge,4);
                Line({P(50,51),P(58,60)},FLinearColor(.67,.39,.13,1),4.5);
                Line({P(55,60),P(60,56)},Ivory,3);
            }
        }
        else if(W->Symbol==1)
        {
            Poly(Oct(23,23,57,55,8),Edge);
            Poly(Oct(25,24,55,53,7),Ivory);
            Poly(Oct(29,28,51,49,5),FLinearColor(.023,.045,.052,1));
            Line({{32,38},{48,38}},Ivory,4);
        }
        else if(W->Symbol==2)
        {
            Line({{29,20},{29,58}},Ivory,3);
            Poly({{31,22},{56,25},{49,33},{56,41},{31,38}},Ivory);
            Line({{23,59},{35,59}},Ivory,3);
        }
        else
        {
            Line({{24,46},{40,30},{56,46}},Ivory,4);
            Line({{27,54},{40,41},{53,54}},FLinearColor(.3,.83,.76,.8),2);
        }
        // A bounded reflection traverses the upper lip while hovered.
        if(HoverAmount>.01f)
        {
            const double T=FMath::Frac(FSlateApplication::Get().GetCurrentTime()*.4);
            const double X=18.+T*38.;
            Line({{X,6},{X+7,6}},FLinearColor(.8,1,.96,.8*HoverAmount),2.2);
        }
        return Layer;
    }
private:
    TWeakObjectPtr<USFCommandButtonArt> Owner;
    float HoverAmount=0,PressAmount=0;
    double LastPressTime=-100.;
    bool bWasPressed=false;
};

void USFCommandButtonArt::Install(UButton* Button,int32 InSymbol)
{
    if(!Button || Cast<USFCommandButtonArt>(Button->GetContent())) return;
    // Keep the authored button, its tooltip and all Blueprint click handlers.
    auto* Content=NewObject<USFCommandButtonArt>(Button);
    Content->OwnerButton=Button;Content->Symbol=InSymbol;
    Content->SetVisibility(ESlateVisibility::HitTestInvisible);
    FButtonStyle Style=Button->GetStyle();
    FSlateBrush Empty;Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
    Style.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty).SetDisabled(Empty);
    Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    Button->SetStyle(Style);Button->SetBackgroundColor(FLinearColor::White);
    Button->SetColorAndOpacity(FLinearColor::White);
    Button->SetContent(Content);
    if(auto* Slot=Cast<UButtonSlot>(Content->Slot))
    {
        Slot->SetPadding(FMargin(0));Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);
    }
}
TSharedRef<SWidget> USFCommandButtonArt::RebuildWidget()
{
    Art=SNew(SSFCommandButtonArt,this);return Art.ToSharedRef();
}
void USFCommandButtonArt::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);Art.Reset();
}
