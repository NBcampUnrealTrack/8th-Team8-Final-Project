#include "SFProductionQueueFeedbackWidget.h"
#include "SFProductionButtonPaint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void USFProductionQueueFeedbackWidget::InitializeContent(UWidget* Content)
{
    OriginalContent=Content;
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget=Root;
    if (Content)
    {
        auto* ContentSlot=Root->AddChildToOverlay(Content);
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceVolatile(true);
}
UWidget* USFProductionQueueFeedbackWidget::ReleaseContent()
{
    if (auto* Root=Cast<UOverlay>(GetRootWidget())) Root->ClearChildren();
    return OriginalContent;
}
void USFProductionQueueFeedbackWidget::SetPresentation(float InHover,float InFlash,bool InOccupied,bool InActive,float InProgress)
{
    Hover=InHover; Flash=InFlash; bOccupied=InOccupied; bActive=InActive; Progress=InProgress;
}
int32 USFProductionQueueFeedbackWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,
    const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    using namespace SFProductionPaint;
    Layer=Super::NativePaint(Args,G,Cull,Out,Layer,Style,bParentEnabled)+1;
    const float Alpha=Style.GetColorAndOpacityTint().A;
    const FVector2D P=FVector2D::ZeroVector;
    const FVector2D S=G.GetLocalSize();
    if (S.X<1 || S.Y<1) return Layer;
    const float Intensity=FMath::Max(Hover,Flash);
    FLinearColor C=Cancel; C.A=Alpha*Intensity;
    Rim(Out,Layer,G,P+FVector2D(4,4),S-FVector2D(8,8),C,2.f);
    if (bOccupied)
    {
        FLinearColor Base=bActive ? Ether : Gold; Base.A=Alpha*(1.f-Hover)*.85f;
        Box(Out,Layer,G,P+FVector2D(9,S.Y-8),FVector2D(S.X-18,2),FLinearColor(.1f,.15f,.13f,Alpha));
        Box(Out,Layer,G,P+FVector2D(9,S.Y-8),FVector2D((S.X-18)*(bActive ? Progress : 1.f),2),Base);
        // Compact status marker: active chevrons / waiting dots; no extra text crowded into the slots.
        const FVector2D Mark=P+FVector2D(S.X*.5f,S.Y+6);
        if (bActive) Line(Out,Layer,G,{Mark-FVector2D(5,2),Mark,Mark-FVector2D(5,-2)},Base,2.f);
        else Box(Out,Layer,G,Mark-FVector2D(1,1),FVector2D(2,2),Base);
    }
    if (Intensity > .01f)
    {
        const FVector2D Center=P+FVector2D(S.X-11,11);
        Box(Out,Layer,G,Center-FVector2D(8,8),FVector2D(16,16),FLinearColor(.13f,.035f,.025f,Alpha*Intensity));
        Line(Out,Layer,G,{Center-FVector2D(3,3),Center+FVector2D(3,3)},C,2.f);
        Line(Out,Layer,G,{Center+FVector2D(-3,3),Center+FVector2D(3,-3)},C,2.f);
    }
    return Layer;
}
