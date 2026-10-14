#include "InGame/HUD/SFHUDPanelAccentWidget.h"
#include "InGame/HUD/SFHUDMotesActor.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InGame/HUD/SFCommandButtonArt.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SSFHUDPanelAccent : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFHUDPanelAccent) {} SLATE_END_ARGS()
    void Construct(const FArguments&, USFHUDPanelAccentWidget* InOwner)
    {
        Owner = InOwner;
        SetCanTick(InOwner->bForeground);
        ForceVolatile(InOwner->bForeground);
    }
    virtual void Tick(const FGeometry& G,double Time,float Delta) override
    {
        auto* W=Owner.Get();
        if(W && IsValid(W->MotesActor)) W->MotesActor->RequestFrame();
        if(W && W->FrameMaterial)
        {
            const float Pulse=FMath::Exp(-FMath::Max(0.,Time-W->PulseStart)*3.1);
            const float Target=W->PanelOwner.IsValid() && W->PanelOwner->IsHovered()?1.f:0.f;
            HoverGlow=FMath::FInterpTo(HoverGlow,Target,Delta,7.f);
            if(FMath::Abs(Pulse-LastPulse)>.004f || FMath::Abs(HoverGlow-LastHover)>.004f)
            {
                W->FrameMaterial->SetScalarParameterValue(TEXT("SelectionPulse"),Pulse);
                W->FrameMaterial->SetScalarParameterValue(TEXT("HoverGlow"),HoverGlow);
                LastPulse=Pulse;LastHover=HoverGlow;
            }
        }
        SLeafWidget::Tick(G,Time,Delta);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const auto* W = Owner.Get();
        if (!W) return Layer;
        const FVector2D Size = G.GetLocalSize();
        if (Size.X < 80. || Size.Y < 80.) return Layer;
        using EPanel = USFHUDPanelAccentWidget::EPanel;
        const bool Mini = W->Panel == EPanel::Minimap;
        const bool Command = W->Panel == EPanel::Command;
        const FMargin Inset = Mini ? FMargin(55, 44, 55, 47)
            : Command ? FMargin(14, 27, 14, 8) : FMargin(26, 36, 26, 26);
        const FVector2D A(Inset.Left, Inset.Top), B(Size.X-Inset.Right, Command ? Size.Y : Size.Y-Inset.Bottom);
        const FLinearColor Tint = Style.GetColorAndOpacityTint();
        auto Line = [&](TArray<FVector2D> Points, FLinearColor Color, float Width = 1.f)
        {
            FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, Color * Tint, true, Width);
        };
        auto Box = [&](FVector2D Pos, FVector2D Extent, FLinearColor Color)
        {
            FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(Extent, FSlateLayoutTransform(Pos)),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color * Tint);
        };
        auto Diamond = [&](FVector2D P, float R, FLinearColor Color, float Width = 1.f)
        {
            Line({P+FVector2D(0,-R),P+FVector2D(R,0),P+FVector2D(0,R),P+FVector2D(-R,0),P+FVector2D(0,-R)},Color,Width);
        };
        const FLinearColor Gold(.74f,.52f,.25f,1.f);
        if (!W->bForeground)
        {
            // Opaque inset backing separates readable UI from moving terrain beneath it.
            for (int32 I=0; I<16; ++I)
            {
                const float T=I/15.f;
                const FLinearColor Color = FMath::Lerp(FLinearColor(.025f,.047f,.052f,.96f),FLinearColor(.008f,.015f,.024f,.99f),T);
                Box(A+FVector2D(0,(B.Y-A.Y)*I/16.),FVector2D(B.X-A.X,(B.Y-A.Y)/16.+.5),Color);
            }
            if (Command)
            {
                // Engraved diagonals and a subdued crest stay behind every button and icon.
                for (double X=A.X; X<B.X-90.; X+=70.)
                    Line({FVector2D(X,B.Y),FVector2D(X+85.,FMath::Max(A.Y,B.Y-85.))},FLinearColor(.3f,.58f,.55f,.055f));
                const FVector2D C(Size.X*.76,Size.Y*.57);
                Diamond(C,58.f,FLinearColor(.22f,.6f,.58f,.09f));
                Diamond(C,43.f,FLinearColor(.55f,.46f,.28f,.12f));
                Line({C+FVector2D(-70,0),C+FVector2D(70,0)},FLinearColor(.4f,.6f,.57f,.07f));
                Line({C+FVector2D(0,-70),C+FVector2D(0,70)},FLinearColor(.4f,.6f,.57f,.07f));
            }
            return Layer;
        }
        // Light follows the artwork's cyan texels in the frame material;
        // screen-space rails drift over wooden bevels at different aspect ratios.
        if (W->Panel==EPanel::Selection && IsValid(W->MotesActor) && W->MotesActor->GetMaterial())
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(W->MotesActor->GetMaterial());
            Brush.ImageSize=FVector2D(180,90);
            // Confine particles to the upper gemstone; never cover text or portrait controls.
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(180,90),
                FSlateLayoutTransform(FVector2D(Size.X*.5-90,-26))),&Brush,
                ESlateDrawEffect::None,Tint);
        }
        if (!Command) return Layer;
        // Solid carved lower rail seals the original transparent bottom strip.
        Box(FVector2D(0,Size.Y-17),FVector2D(Size.X,17),FLinearColor(.026f,.015f,.009f,1));
        Box(FVector2D(0,Size.Y-14),FVector2D(Size.X,8),FLinearColor(.16f,.068f,.024f,1));
        Line({FVector2D(0,Size.Y-15),FVector2D(Size.X,Size.Y-15)},FLinearColor(.67f,.42f,.16f,.85f),1.5f);
        Line({FVector2D(0,Size.Y-6),FVector2D(Size.X,Size.Y-6)},FLinearColor(.31f,.15f,.049f,1),1.f);
        Line({FVector2D(0,Size.Y-2),FVector2D(Size.X,Size.Y-2)},FLinearColor(.008f,.005f,.003f,1),3.f);
        if (Command)
        {
            for (int32 I=-3;I<=3;++I)
            {
                const double X=Size.X*.5+I*12;
                Line({FVector2D(X,B.Y-9),FVector2D(X,B.Y-(I==0?16:12))},FLinearColor(Gold.R,Gold.G,Gold.B,.3f));
            }
        }
        return Layer;
    }
private:
    TWeakObjectPtr<USFHUDPanelAccentWidget> Owner;
    float HoverGlow=0.f,LastPulse=-1.f,LastHover=-1.f;
};

void USFHUDPanelAccentWidget::Pulse() { PulseStart=FSlateApplication::Get().GetCurrentTime(); }
TSharedRef<SWidget> USFHUDPanelAccentWidget::RebuildWidget()
{
    if(bForeground && Panel==EPanel::Selection && GetWorld() && GetWorld()->IsGameWorld() && !IsValid(MotesActor))
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags|=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        MotesActor=GetWorld()->SpawnActor<ASFHUDMotesActor>(FVector(0,0,-200000),FRotator::ZeroRotator,Params);
    }
    AccentSlate=SNew(SSFHUDPanelAccent,this);
    return AccentSlate.ToSharedRef();
}
void USFHUDPanelAccentWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    AccentSlate.Reset();
    if(IsValid(MotesActor)) MotesActor->Destroy();
    MotesActor=nullptr;
}

void USFHUDPanelAccentWidget::Install(UUserWidget* Root,TArray<TWeakObjectPtr<USFHUDPanelAccentWidget>>& Accents)
{
    TSet<UUserWidget*> Visited;
    TFunction<void(UUserWidget*)> Visit = [&](UUserWidget* User)
    {
        if (!User || !User->WidgetTree || Visited.Contains(User)) return;
        Visited.Add(User);
        const FName ButtonNames[]={TEXT("AttackCommandButton"),TEXT("StopCommandButton"),TEXT("RallyPointButton"),TEXT("BuildModeOpenButton")};
        for(int32 Index=0;Index<UE_ARRAY_COUNT(ButtonNames);++Index)
            USFCommandButtonArt::Install(Cast<UButton>(User->GetWidgetFromName(ButtonNames[Index])),Index);
        TArray<UWidget*> Widgets;
        User->WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            if (auto* Child=Cast<UUserWidget>(Widget)) Visit(Child);
            if (auto* Stack=Cast<UCommonActivatableWidgetContainerBase>(Widget)) Visit(Stack->GetActiveWidget());
        }
        if (User->GetWidgetFromName(TEXT("SFPanelAccentFront"))) return;
        const TPair<FName,EPanel> Frames[] = {
            {TEXT("SelectionPanelFrame"),EPanel::Selection}, {TEXT("MinimapImage"),EPanel::Minimap},
            {TEXT("Image_221"),EPanel::Command}, {TEXT("ProductionPanelFrame"),EPanel::Command},
            {TEXT("BuildingmodeFrame"),EPanel::Command}};
        for (const auto& Frame : Frames)
        {
            auto* Image=Cast<UImage>(User->GetWidgetFromName(Frame.Key));
            auto* Canvas=Image?Cast<UCanvasPanel>(Image->GetParent()):nullptr;
            auto* FrameSlot=Image?Cast<UCanvasPanelSlot>(Image->Slot):nullptr;
            if (!Canvas || !FrameSlot) continue;
            if (Frame.Value == EPanel::Command)
            {
                // The original frame starts at -15 but was only 300 high in a
                // 300-high panel, leaving a 15-unit opening at the viewport edge.
                FVector2D Size=FrameSlot->GetSize();
                Size.Y=FMath::Max(Size.Y,315.);
                FrameSlot->SetSize(Size);
                if (FrameSlot->GetAlignment().Y > .9f && FrameSlot->GetAnchors().Minimum.Y > .9f)
                {
                    FVector2D Position=FrameSlot->GetPosition();
                    Position.Y=0.;
                    FrameSlot->SetPosition(Position);
                }
            }
            for (int32 Pass=0;Pass<2;++Pass)
            {
                auto* Accent=User->WidgetTree->ConstructWidget<USFHUDPanelAccentWidget>(StaticClass(),Pass?TEXT("SFPanelAccentFront"):TEXT("SFPanelAccentBack"));
                Accent->Panel=Frame.Value;Accent->bForeground=Pass!=0;Accent->PanelOwner=User;
                if(Pass)Accent->FrameMaterial=Image->GetDynamicMaterial();
                Accent->SetVisibility(ESlateVisibility::HitTestInvisible);
                auto* Slot=Canvas->AddChildToCanvas(Accent);
                Slot->SetLayout(FrameSlot->GetLayout());
                Slot->SetZOrder(Pass?50:-10);
                Accents.Add(Accent);
            }
            break;
        }
    };
    Accents.RemoveAll([](const auto& Accent){return !Accent.IsValid();});
    Visit(Root);
}
