#include "SFStartButtonFeedbackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Rendering/DrawElements.h"

UTextBlock* USFStartButtonFeedbackWidget::CreateLabel()
{
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget=Root;
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();
    auto* LabelSlot=Root->AddChildToOverlay(Label);
    LabelSlot->SetHorizontalAlignment(HAlign_Center);
    LabelSlot->SetVerticalAlignment(VAlign_Center);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceVolatile(true);
    return Label;
}

void USFStartButtonFeedbackWidget::SetFeedback(float InHover,float InHoverTime,float InClickTime,bool bInDrawFeedback)
{
    Hover=InHover;
    HoverTime=InHoverTime;
    ClickTime=InClickTime;
    bDrawFeedback=bInDrawFeedback;
}

int32 USFStartButtonFeedbackWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,
    const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,
    const FWidgetStyle& Style,bool bParentEnabled) const
{
    Layer=Super::NativePaint(Args,G,Cull,Out,Layer,Style,bParentEnabled);
    const float Pulse=FMath::Clamp(ClickTime/.42f,0.f,1.f);
    if (!bDrawFeedback || !bParentEnabled || (Hover<.01f && Pulse>=1.f)) return Layer;
    ++Layer;
    const float Alpha=Style.GetColorAndOpacityTint().A;
    auto Line=[&](TArray<FVector2D> Points,FLinearColor Color,float Thickness=1.f)
    { Color.A*=Alpha; FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness); };
    const FVector2D P=G.GetLocalSize()*.5f;
    const FLinearColor Teal(.08f,.88f,.68f,1.f);
    const FLinearColor Pearl(.58f,1.f,.9f,1.f);
    auto Glow=[&](const TArray<FVector2D>& Points,float Opacity,float Width=1.f)
    {
        FLinearColor C=Teal; C.A=Opacity*.075f; Line(Points,C,Width+9.f);
        C.A=Opacity*.18f; Line(Points,C,Width+4.f);
        C=Pearl; C.A=Opacity; Line(Points,C,Width);
    };
    // Short, opposing wisps above and below the label. A pause between sweeps
    // keeps this closer to drifting magical light than a continuously scanning HUD.
    const float Breath=.83f+.17f*FMath::Sin(HoverTime*3.f);
    for (int32 Side : {-1,1})
    {
        const float Y=Side*25.f;
        Glow({P+FVector2D(-88.f*Hover,Y),P+FVector2D(88.f*Hover,Y)},
            Hover*.25f*Breath);
        const float Cycle=FMath::Fmod(HoverTime,1.85f)/1.15f;
        if (Cycle<1.f)
        {
            const float Head=FMath::Lerp(-112.f,112.f,Cycle);
            const float Fade=FMath::Sin(Cycle*PI)*Hover;
            for (int32 Segment=0;Segment<8;++Segment)
            {
                const float X=Head-Segment*5.f;
                if (X < -100.f || X > 100.f) continue;
                Glow({P+FVector2D(Side*X,Y),P+FVector2D(Side*(X-4.f),Y)},
                    Fade*(1.f-Segment/8.f),1.35f);
            }
        }
    }
    const FVector2D D=P-FVector2D(112.f+4.f*(1.f-Hover),0);
    const float R=4.f+Hover*.8f;
    Glow({D-FVector2D(R,0),D-FVector2D(0,R),D+FVector2D(R,0),D+FVector2D(0,R),D-FVector2D(R,0)},
        Hover*.9f*Breath,1.2f);
    if (Pulse<1.f)
    {
        // A brief outward ripple on press, including keyboard/controller activation.
        const float Fade=FMath::Square(1.f-Pulse);
        for (int32 Side : {-1,1})
        {
            const float X=Side*(85.f+42.f*Pulse);
            const float SparkRadius=.8f+4.f*Fade;
            const FVector2D Spark=P+FVector2D(X,0);
            Glow({Spark-FVector2D(SparkRadius*2.f,0),Spark+FVector2D(SparkRadius*2.f,0)},Fade,1.5f);
            Glow({Spark-FVector2D(0,SparkRadius),Spark+FVector2D(0,SparkRadius)},Fade,1.5f);
            const float Y=Side*(20.f+8.f*Pulse);
            Glow({P+FVector2D(-85.f-15.f*Pulse,Y),P+FVector2D(85.f+15.f*Pulse,Y)},Fade*.75f,1.6f);
        }
    }
    return Layer;
}
