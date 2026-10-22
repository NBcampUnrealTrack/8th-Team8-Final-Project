#include "SFPVPLobbyWidget.h"
#include "Game/Multiplayer/SFPVPSessionSubsystem.h"
#include "Game/Multiplayer/SFPVPLobby.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"

namespace SFPVPUI
{
    const FLinearColor Gold(.78f,.53f,.23f), Cream(.98f,.90f,.70f), Teal(.21f,.83f,.78f), Muted(.64f,.66f,.53f);

    static void Polygon(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,const TArray<FVector2D>& Points,FLinearColor Color)
    {
        const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
        const auto* Proxy=Resource.GetResourceProxy();const FVector2f UV=Proxy?Proxy->StartUV+Proxy->SizeUV*.5f:FVector2f(.5f,.5f);
        TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
        for(const auto P:Points)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),UV,Color.ToFColor(true)));
        for(int32 I=1;I+1<Vertices.Num();++I)Indices.Append({0,static_cast<SlateIndex>(I),static_cast<SlateIndex>(I+1)});
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,Vertices,Indices,nullptr,0,0);
    }
    static TArray<FVector2D> Bevel(FVector2D Size,float Inset,float Cut)
    {
        const float R=Size.X-Inset,B=Size.Y-Inset;
        return {{Inset+Cut,Inset},{R-Cut,Inset},{R,Inset+Cut},{R,B-Cut},{R-Cut,B},{Inset+Cut,B},{Inset,B-Cut},{Inset,Inset+Cut}};
    }
    static void Gem(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,FVector2D C,float R,FLinearColor Tint)
    {
        Polygon(G,Out,Layer,{C+FVector2D(0,-R-3),C+FVector2D(R+3,0),C+FVector2D(0,R+3),C+FVector2D(-R-3,0)},Gold*Tint);
        Polygon(G,Out,Layer+1,{C+FVector2D(0,-R),C+FVector2D(R,0),C+FVector2D(0,R),C+FVector2D(-R,0)},FLinearColor(.015f,.21f,.22f)*Tint);
        Polygon(G,Out,Layer+2,{C+FVector2D(0,-R),C+FVector2D(R*.65f,0),C,C+FVector2D(-R,0)},FLinearColor(.34f,.98f,.94f)*Tint);
        Polygon(G,Out,Layer+2,{C,C+FVector2D(R*.65f,0),C+FVector2D(0,R),C+FVector2D(-R,0)},Teal*Tint);
    }

    // Match the campaign's carved gold bevels and ether inlays at each button's real size.
    class SForgeButton : public SButton
    {
    public:
        using FArguments=SButton::FArguments;
        void Construct(const FArguments& Args)
        {
            Face=Args._ButtonStyle->Normal.TintColor.GetSpecifiedColor();
            Transparent=*Args._ButtonStyle;
            Transparent.Normal.DrawAs=Transparent.Hovered.DrawAs=Transparent.Pressed.DrawAs=Transparent.Disabled.DrawAs=ESlateBrushDrawType::NoDrawType;
            auto Clean=Args;Clean.ButtonStyle(&Transparent);SButton::Construct(Clean);
        }
        virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& WidgetStyle,bool ParentEnabled) const override
        {
            const FVector2D Size=G.GetLocalSize();const bool Enabled=ParentEnabled&&IsEnabled();
            FLinearColor Tint=WidgetStyle.GetColorAndOpacityTint();if(!Enabled)Tint.A*=.42f;
            const float Cut=FMath::Min(10.f,Size.Y*.18f);
            Polygon(G,Out,Layer,Bevel(Size,0,Cut),FLinearColor(.12f,.052f,.015f)*Tint);
            Polygon(G,Out,Layer+1,Bevel(Size,1.5f,Cut),Gold*Tint);
            Polygon(G,Out,Layer+2,Bevel(Size,4.5f,Cut*.75f),(IsHovered()&&Enabled?FLinearColor(.035f,.15f,.13f):Face)*Tint);
            const FLinearColor Light=IsPressed()?FLinearColor(.55f,.33f,.12f):FLinearColor(1.f,.82f,.46f);
            FSlateDrawElement::MakeLines(Out,Layer+3,G.ToPaintGeometry(),TArray<FVector2D>{{3,Cut+1},{Cut+1,3},{Size.X-Cut-1,3},{Size.X-3,Cut+1}},ESlateDrawEffect::None,Light*Tint,true,1.5f);
            FSlateDrawElement::MakeLines(Out,Layer+3,G.ToPaintGeometry(),TArray<FVector2D>{{Cut,Size.Y-3},{Size.X-Cut,Size.Y-3},{Size.X-3,Size.Y-Cut}},ESlateDrawEffect::None,FLinearColor(.36f,.19f,.055f)*Tint,true,2.f);
            if(Size.Y>43&&Size.X>115)
            {Gem(G,Out,Layer+3,FVector2D(10,Size.Y*.5),3.5f,Tint);Gem(G,Out,Layer+3,FVector2D(Size.X-10,Size.Y*.5),3.5f,Tint);}
            return SButton::OnPaint(Args,G,Clip,Out,Layer+6,WidgetStyle,ParentEnabled);
        }
    private:
        FButtonStyle Transparent;FLinearColor Face;
    };

    // One geometric inlay language throughout the lobby; no stretched raster frames.
    class STrim : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STrim) : _Accent(Gold), _FullFrame(false) {}
            SLATE_ARGUMENT(FLinearColor, Accent)
            SLATE_ARGUMENT(bool, FullFrame)
        SLATE_END_ARGS()
        void Construct(const FArguments& Args) { Accent=Args._Accent; bFullFrame=Args._FullFrame; SetVisibility(EVisibility::HitTestInvisible); }
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(32); }
        virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const override
        {
            const FVector2D Size=G.GetLocalSize();
            const FLinearColor Tint=Style.GetColorAndOpacityTint();
            if(bFullFrame)
            {
                auto Rail=Bevel(Size,4,16);const FVector2D First=Rail[0];Rail.Add(First);
                FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Rail,ESlateDrawEffect::None,FLinearColor(.12f,.052f,.018f)*Tint,true,11.f);
                FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Rail,ESlateDrawEffect::None,Gold*Tint,true,5.f);
                FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),TArray<FVector2D>{{7,21},{21,7},{Size.X-21,7},{Size.X-7,21}},ESlateDrawEffect::None,FLinearColor(1,.83f,.48f)*Tint,true,1.8f);
                for(const FVector2D C:{FVector2D(20,20),FVector2D(Size.X-20,20),FVector2D(20,Size.Y-20),Size-FVector2D(20)})Gem(G,Out,Layer+3,C,7,Tint);
                return Layer+6;
            }
            for(int32 X=0;X<2;++X) for(int32 Y=0;Y<2;++Y)
            {
                TArray<FVector2D> Points;
                for(const FVector2D P : {FVector2D(9,35),FVector2D(9,18),FVector2D(18,9),FVector2D(35,9)})
                    Points.Add(FVector2D(X?Size.X-P.X:P.X,Y?Size.Y-P.Y:P.Y));
                FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Accent*Tint,true,2.f);
            }
            const float C=Size.X*.5f;
            TArray<FVector2D> Diamond={{C,5},{C+4,9},{C,13},{C-4,9},{C,5}};
            FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,Teal*Tint,true,1.f);
            return Layer;
        }
    private:
        FLinearColor Accent;
        bool bFullFrame=false;
    };

    class SRune : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SRune) : _Accent(Teal), _CrossedBlades(true) { _Visibility=EVisibility::HitTestInvisible; }
            SLATE_ARGUMENT(FLinearColor, Accent)
            SLATE_ARGUMENT(bool, CrossedBlades)
        SLATE_END_ARGS()
        void Construct(const FArguments& Args) { Accent=Args._Accent; bCrossedBlades=Args._CrossedBlades; }
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(80); }
        virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const override
        {
            const FVector2D C=G.GetLocalSize()*.5f;
            const float R=FMath::Min(C.X,C.Y)*.72f;
            auto Line=[&](TArray<FVector2D> P,FLinearColor Color,float Width=1.f)
            { FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint(),true,Width); };
            for(const float Scale : {1.f,.78f})
                Line({C+FVector2D(0,-R*Scale),C+FVector2D(R*Scale,0),C+FVector2D(0,R*Scale),C+FVector2D(-R*Scale,0),C+FVector2D(0,-R*Scale)},FLinearColor(Accent.R,Accent.G,Accent.B,Scale==1.f?.65f:.24f));
            if(bCrossedBlades)
            {
                Line({C+FVector2D(-R*.34f,R*.34f),C+FVector2D(R*.34f,-R*.34f)},Accent,2.f);
                Line({C+FVector2D(-R*.34f,-R*.34f),C+FVector2D(R*.34f,R*.34f)},Accent,2.f);
                Line({C+FVector2D(-R*.46f,R*.16f),C+FVector2D(-R*.16f,R*.46f)},Gold,2.f);
                Line({C+FVector2D(R*.16f,R*.46f),C+FVector2D(R*.46f,R*.16f)},Gold,2.f);
            }
            return Layer;
        }
    private:
        FLinearColor Accent;
        bool bCrossedBlades=true;
    };
}

// Screen-space motes stay inexpensive and independent of a world camera or Niagara scene.
class SSFPVPAmbience : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFPVPAmbience) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) { SetVisibility(EVisibility::HitTestInvisible); ForceVolatile(true); }
    void Advance(float Delta) { Time=FMath::Fmod(Time+Delta,1800.f); Invalidate(EInvalidateWidgetReason::Paint); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const override
    {
        const FVector2D Size=G.GetLocalSize();
        auto Box=[&](FVector2D P,FVector2D Extent,FLinearColor Color,const FSlateBrush* Brush)
        {FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(Extent,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint());};
        const FLinearColor Tint=Style.GetColorAndOpacityTint();
        TArray<FSlateGradientStop> Shade={
            FSlateGradientStop(FVector2D(0,0),FLinearColor(.006f,.019f,.015f,.61f)*Tint),
            FSlateGradientStop(FVector2D(0,Size.Y*.45f),FLinearColor(.006f,.019f,.015f,.15f)*Tint),
            FSlateGradientStop(FVector2D(0,Size.Y),FLinearColor(.006f,.019f,.015f,.61f)*Tint)};
        FSlateDrawElement::MakeGradient(Out,Layer,G.ToPaintGeometry(),Shade,Orient_Vertical);
        for(int32 I=0;I<26;++I)
        {
            const float Seed=I*1.618034f;
            const float Side=(I%2)?(.89f+FMath::Frac(Seed)*.10f):(.01f+FMath::Frac(Seed)*.10f);
            const float Y=1.f-FMath::Frac(Seed*.73f+Time*(.009f+(I%4)*.002f));
            const FVector2D P((Side+FMath::Sin(Time*.3f+I)*.007f)*Size.X,Y*Size.Y);
            const float Alpha=.18f+.22f*(.5f+.5f*FMath::Sin(Time*.8f+I*2.f));
            const FLinearColor C=I%3?SFPVPUI::Teal:SFPVPUI::Gold;
            Box(P-FVector2D(7),FVector2D(14),FLinearColor(C.R,C.G,C.B,Alpha*.1f),&Glow);
            Box(P-FVector2D(1.3f),FVector2D(2.6f),FLinearColor(C.R,C.G,C.B,Alpha),&Dot);
        }
        return Layer;
    }
private:
    float Time=0;
    FSlateRoundedBoxBrush Glow{FLinearColor::White,7.f};
    FSlateRoundedBoxBrush Dot{FLinearColor::White,2.f};
};

USFPVPLobbyWidget::USFPVPLobbyWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font")); Font=F.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> E(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Etherial/Commander/T_EtherCommander_Normal.T_EtherCommander_Normal")); EtherPortrait=E.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> B(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Beast/Commander/Beast_Commander_Smile.Beast_Commander_Smile")); BeastPortrait=B.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> Camp(TEXT("/Game/Core/UI/LobbyAtmosphere/T_LobbyLowPolyCamp.T_LobbyLowPolyCamp")); CampArt=Camp.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> Logo(TEXT("/Game/Core/UI/LobbyAtmosphere/T_StartEtherOath.T_StartEtherOath")); Wordmark=Logo.Object;
    SetIsFocusable(true);
}

USFPVPSessionSubsystem* USFPVPLobbyWidget::Sessions() const { return GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>(); }
bool USFPVPLobbyWidget::InRoom() const
{
    const auto* State=GetWorld()?GetWorld()->GetGameState<ASFPVPLobbyState>():nullptr;
    return GetWorld()&&(GetWorld()->GetNetMode()!=NM_Standalone||(State&&State->bAIEnabled));
}

TSharedRef<SWidget> USFPVPLobbyWidget::Label(const FString& Text,int32 Size,FLinearColor Color) const
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(FSlateFontInfo(Font,Size)).ColorAndOpacity(Color)
        .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.55f)).Justification(ETextJustify::Center);
}

TSharedRef<SWidget> USFPVPLobbyWidget::PlayerCard(int32 Seat)
{
    using namespace SFPVPUI;
    const auto Player=[this,Seat]() -> ASFPVPLobbyPlayerState*
    { const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>(); return S?S->GetSeat(Seat):nullptr; };
    return SNew(SOverlay)
    +SOverlay::Slot()[SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(26,16))
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)
                [SNew(STextBlock).Font(FSlateFontInfo(Font,19)).ColorAndOpacity(Muted)
                    .Text_Lambda([Player,Seat]{const auto* P=Player();return FText::FromString(Seat==0?TEXT("01  /  방장"):P&&P->IsABot()?TEXT("02  /  인공지능"):TEXT("02  /  도전자"));})]
                +SHorizontalBox::Slot().AutoWidth()
                [SNew(STextBlock).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Teal).Text_Lambda([this,Seat]
                {const auto* PC=GetOwningPlayer();const auto* P=PC?PC->GetPlayerState<ASFPVPLobbyPlayerState>():nullptr;return FText::FromString(P&&P->Seat==Seat?TEXT("내 자리"):TEXT(""));})]]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,4)
            [SNew(SBox).WidthOverride(94).HeightOverride(94)
                [SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SBorder).BorderImage(&InsetBrush).Padding(8)
                        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                            [SNew(SImage).Image_Lambda([this,Player]() -> const FSlateBrush* {const auto* P=Player();return P&&P->Race==ESFRace::Beast?&BeastBrush:&EtherBrush;})
                                .ColorAndOpacity_Lambda([Player]{const auto* P=Player();return P&&!P->bRandomRace?FLinearColor::White:FLinearColor(0,0,0,0);})]]]
                    +SOverlay::Slot().Padding(8)[SNew(SRune).Accent(Gold).CrossedBlades(false)
                        .Visibility_Lambda([Player]{const auto* P=Player();return !P||P->bRandomRace?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
                    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                        [SNew(STextBlock).Font(FSlateFontInfo(Font,66)).ColorAndOpacity(Cream).Text(FText::FromString(TEXT("?")))
                            .Visibility_Lambda([Player]{const auto* P=Player();return P&&P->bRandomRace?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]]]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Font(FSlateFontInfo(Font,27)).ColorAndOpacity(Cream).Justification(ETextJustify::Center)
                .Text_Lambda([Player]{const auto* P=Player();return FText::FromString(!P?TEXT("도전자를 기다립니다"):P->bRandomRace?TEXT("랜덤"):P->Race==ESFRace::Beast?TEXT("비스트"):TEXT("에테리안"));})]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,6)
            [SNew(STextBlock).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .Text_Lambda([Player]{const auto* P=Player();return FText::FromString(!P?TEXT("초대하거나 입장을 기다려 주세요"):P->bRandomRace?TEXT("전투 시작 시 종족이 결정됩니다"):P->GetPlayerName());})]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SBorder).BorderImage(&InsetBrush).Padding(FMargin(18,5))
                [SNew(STextBlock).Font(FSlateFontInfo(Font,18)).Justification(ETextJustify::Center)
                    .ColorAndOpacity_Lambda([Player]{const auto* P=Player();return P&&P->bReady?Teal:Muted;})
                            .Text_Lambda([Player]{const auto* P=Player();return FText::FromString(!P?TEXT("빈 자리"):P->bReady?TEXT("준비 완료"):TEXT("준비 중"));})]]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,6,0,0)
            [SNew(SHorizontalBox).Visibility_Lambda([this,Seat,Player]
                {const auto* P=GetOwningPlayer()->GetPlayerState<ASFPVPLobbyPlayerState>();const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();const auto* G=Player();
                return Seat==1&&P&&P->Seat==0&&S&&!S->bStarting&&(!G||G->IsABot())?EVisibility::Visible:EVisibility::Collapsed;})
                +SHorizontalBox::Slot().AutoWidth()
                [SNew(SFPVPUI::SForgeButton).ButtonStyle(&SmallButtonStyle).ContentPadding(FMargin(12,5))
                    .OnClicked_Lambda([this,Player]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerSetAIOpponent(!Player());return FReply::Handled();})
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,16)).ColorAndOpacity(Teal)
                        .Text_Lambda([Player]{return FText::FromString(Player()?TEXT("AI 제거"):TEXT("AI 추가"));})]]
                +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
                [SNew(SFPVPUI::SForgeButton).ButtonStyle(&SmallButtonStyle).ContentPadding(FMargin(12,5))
                    .Visibility_Lambda([Player]{return Player()?EVisibility::Visible:EVisibility::Collapsed;})
                    .OnClicked_Lambda([this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerCycleAIRace();return FReply::Handled();})
                    [Label(TEXT("AI 종족 변경"),16,Cream)]]]]]
    +SOverlay::Slot()[SNew(STrim).Accent(Seat==0?Teal:Gold)];
}

TSharedRef<SWidget> USFPVPLobbyWidget::RebuildWidget()
{
    using namespace SFPVPUI;
    EtherBrush.SetResourceObject(EtherPortrait); BeastBrush.SetResourceObject(BeastPortrait);
    EtherBrush.ImageSize=BeastBrush.ImageSize=FVector2D(190);
    CampBrush.SetResourceObject(CampArt); CampBrush.ImageSize=FVector2D(1672,941);
    LogoBrush.SetResourceObject(Wordmark); LogoBrush.ImageSize=FVector2D(1050,350);
    PanelBrush=FSlateRoundedBoxBrush(FLinearColor(.028f,.031f,.022f,.97f),8.f,FLinearColor(.38f,.24f,.10f,.95f),1.f);
    InsetBrush=FSlateRoundedBoxBrush(FLinearColor(.009f,.025f,.022f,.95f),4.f,FLinearColor(.32f,.24f,.12f,.85f),1.f);
    CardBrush=FSlateRoundedBoxBrush(FLinearColor(.023f,.044f,.034f,.97f),6.f,FLinearColor(.50f,.35f,.16f),1.5f);
    ButtonStyle=FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.016f,.039f,.033f),4.f,Gold,1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.075f,.16f,.125f),4.f,Teal,1.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.025f,.07f,.055f),4.f,Teal,1.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.025f,.035f,.03f),4.f,FLinearColor(.1f,.14f,.11f),1.f))
        .SetNormalPadding(FMargin(22,12)).SetPressedPadding(FMargin(22,13,22,11));
    PrimaryButtonStyle=ButtonStyle;
    SmallButtonStyle=ButtonStyle;SmallButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,1,0,0));
    PrimaryButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.028f,.13f,.10f),4.f,Gold,1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.10f,.34f,.26f),4.f,Teal,1.5f));
    TabButtonStyle=ButtonStyle;
    TabButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.025f,.047f,.038f),3.f,FLinearColor(.17f,.24f,.19f),1.f))
        .SetNormalPadding(FMargin(24,10)).SetPressedPadding(FMargin(24,11,24,9));
    InputStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    InputStyle.SetFont(FSlateFontInfo(Font,21)).SetForegroundColor(Cream)
        .SetBackgroundImageNormal(InsetBrush)
        .SetBackgroundImageHovered(FSlateRoundedBoxBrush(FLinearColor(.014f,.033f,.026f),4.f,Muted,1.f))
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FLinearColor(.014f,.033f,.026f),4.f,Teal,1.f))
        .SetBackgroundImageReadOnly(InsetBrush).SetPadding(FMargin(16,10));

    const auto Button=[this](const TCHAR* Text,TFunction<void()> Action,bool bPrimary=false,bool bNeedsLogin=false) -> TSharedRef<SWidget>
    {return SNew(SFPVPUI::SForgeButton).ButtonStyle(bPrimary?&PrimaryButtonStyle:&ButtonStyle).HAlign(HAlign_Center).VAlign(VAlign_Center)
        .IsEnabled_Lambda([this,bNeedsLogin]{return !Sessions()->IsBusy()&&(!bNeedsLogin||!Sessions()->IsInternetMode()||Sessions()->IsInternetLoggedIn());})
        .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text,21,Cream)];};
    const auto Tab=[this](const TCHAR* Text,bool bInternet) -> TSharedRef<SWidget>
    {return SNew(SFPVPUI::SForgeButton).ButtonStyle(&TabButtonStyle).IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})
        .OnClicked_Lambda([this,bInternet]{Sessions()->SetInternetMode(bInternet);return FReply::Handled();})
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Text)).Font(FSlateFontInfo(Font,20))
                .ColorAndOpacity_Lambda([this,bInternet]{return Sessions()->IsInternetMode()==bInternet?Teal:Muted;})]
            +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[SNew(SBox).HeightOverride(2)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
                    .BorderBackgroundColor_Lambda([this,bInternet]{return Sessions()->IsInternetMode()==bInternet?Teal:FLinearColor::Transparent;})]]];};

    return SNew(SOverlay)
    +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[SNew(SImage).Image(&CampBrush)]]
    +SOverlay::Slot()[SAssignNew(Ambience,SSFPVPAmbience)]
    +SOverlay::Slot().Padding(32)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
    [SNew(SBox).WidthOverride(1180).HeightOverride(780)
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(14,0,14,18)
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(246).HeightOverride(78)[SNew(SImage).Image(&LogoBrush)]]
            +SHorizontalBox::Slot().AutoWidth().Padding(28,12).VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(1).HeightOverride(48)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Gold*.65f)]]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("결투의 전당"))).Font(FSlateFontInfo(Font,36)).ColorAndOpacity(Cream)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("지휘관과 지휘관, 단 하나의 승부"))).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Cream*.72f)]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Label(TEXT("P V P"),27,Cream)]
                +SVerticalBox::Slot().AutoHeight()[Label(TEXT("1  V S  1"),16,Teal)]]]
        +SVerticalBox::Slot().FillHeight(1)
        [SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBorder).BorderImage(&PanelBrush).Padding(30)
            [SNew(SOverlay)
                +SOverlay::Slot()[SNew(SVerticalBox).Visibility_Lambda([this]{return InRoom()?EVisibility::Collapsed:EVisibility::Visible;})
                    +SVerticalBox::Slot().AutoHeight()
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("인터넷 대전"),true)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[Tab(TEXT("같은 네트워크"),false)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)[Button(TEXT("AI 연습"),[this]{Sessions()->HostPractice();},true)]
                        +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)
                        [SNew(SFPVPUI::SForgeButton).ButtonStyle(&TabButtonStyle)
                            .Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::Visible:EVisibility::Collapsed;})
                            .IsEnabled_Lambda([this]{return !Sessions()->IsBusy()&&Sessions()->IsInternetConfigured()&&Sessions()->IsInternetPlaySupported()&&!Sessions()->IsInternetLoggedIn();})
                            .OnClicked_Lambda([this]{Sessions()->LoginInternet();return FReply::Handled();})
                            [SNew(STextBlock).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Teal)
                                .Text_Lambda([this]{return FText::FromString(Sessions()->GetIdentityLabel());})]]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,18)
                        [SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0).BorderBackgroundColor(FLinearColor(.19f,.25f,.19f))]]
                    +SVerticalBox::Slot().FillHeight(1)
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,26,0)
                        [SNew(SBox).WidthOverride(246)
                        [SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Label(TEXT("지휘관의 선택"),27,Cream)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,10)[Label(TEXT("두 종족, 하나의 전장"),17,Muted)]
                            +SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)
                            [SNew(SBox).HeightOverride(200)
                                [SNew(SHorizontalBox)
                                    +SHorizontalBox::Slot().FillWidth(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&EtherBrush)]]
                                    +SHorizontalBox::Slot().FillWidth(1).Padding(-16,0,0,0)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&BeastBrush)]]]]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Label(TEXT("무작위 전장"),24,Teal)]
                            +SVerticalBox::Slot().AutoHeight()[Label(TEXT("4개의 전장 중 한 곳에서\n상대와 전투를 시작합니다."),17,Muted)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,4)[Label(TEXT("1 · 2 · 4 · 7번 전장"),15,Gold)]]]
                        +SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                            [SNew(STextBlock).Text(FText::FromString(TEXT("새로운 결투 만들기"))).Font(FSlateFontInfo(Font,18)).ColorAndOpacity(Muted)]
                            +SVerticalBox::Slot().AutoHeight()
                            [SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)
                                    [SAssignNew(RoomInput,SEditableTextBox).Style(&InputStyle).Text(FText::FromString(TEXT("StrikeForge 1대1")))
                                        .HintText(FText::FromString(TEXT("방 이름"))).IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})]
                                +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("방 만들기"),[this]{Sessions()->Host(RoomInput->GetText().ToString());},true,true)]]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,22,0,10)
                            [SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                                [SNew(STextBlock).Font(FSlateFontInfo(Font,22)).ColorAndOpacity(Cream)
                                    .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("참가 가능한 방  /  %02d"),Sessions()->GetRooms().Num()));})]
                                +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("새로고침"),[this]{Sessions()->FindRooms();},false,true)]]
                            +SVerticalBox::Slot().FillHeight(1)
                            [SNew(SBorder).BorderImage(&InsetBrush).Padding(18).Clipping(EWidgetClipping::ClipToBounds)
                                [SNew(SOverlay)
                                    +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                                    .Visibility_Lambda([this]{return Sessions()->GetRooms().IsEmpty()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                                    [SNew(SBox).WidthOverride(650).HeightOverride_Lambda([this]{return FOptionalSize(Sessions()->IsInternetMode()?172.f:76.f);})
                                    [SNew(SVerticalBox)
                                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                                        [SNew(SBox).WidthOverride(74).HeightOverride(74)
                                            .Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                                            [SNew(SRune).Accent(Gold)]]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,6)[Label(TEXT("새로운 도전을 기다립니다"),23,Cream)]
                                        +SVerticalBox::Slot().AutoHeight()[Label(TEXT("방을 만들거나 새로고침으로 상대를 찾아보세요."),17,Muted)]]]]
                                    +SOverlay::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(RoomList,SVerticalBox)]]]]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
                            [SNew(SHorizontalBox).Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::Collapsed:EVisibility::Visible;})
                                +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,10,0)
                                [SAssignNew(AddressInput,SEditableTextBox).Style(&InputStyle).HintText(FText::FromString(TEXT("방장 IP 주소  예: 192.168.0.10:7777")))
                                    .IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})]
                                +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("주소로 참가"),[this]{Sessions()->ConnectAddress(AddressInput->GetText().ToString());})]]]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,15)).ColorAndOpacity(Muted).Justification(ETextJustify::Right)
                        .Text_Lambda([this]{return FText::FromString(Sessions()->GetConnectionDescription());})]]
                +SOverlay::Slot()[SNew(SVerticalBox).Visibility_Lambda([this]{return InRoom()?EVisibility::Visible:EVisibility::Collapsed;})
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                        [SNew(STextBlock).Font(FSlateFontInfo(Font,24)).ColorAndOpacity(Cream)
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                            .Text_Lambda([this]{const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();return FText::FromString(!S?TEXT("접속 중"):S->RoomName.IsEmpty()?TEXT("친선 결투"):S->RoomName);})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(18,0,0,0)
                            [SNew(SBox).Visibility_Lambda([this]{const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();return Sessions()->IsInternetMode()&&S&&!S->bAIEnabled?EVisibility::Visible:EVisibility::Collapsed;})
                                [Button(TEXT("Steam 친구 초대"),[this]{Sessions()->ShowSteamInvite();})]]]
                    +SVerticalBox::Slot().FillHeight(1)
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[PlayerCard(0)]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18,0)
                            [SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(66).HeightOverride(66)[SNew(SRune).Accent(Gold)]]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,2)[Label(TEXT("VS"),26,Cream)]]
                        +SHorizontalBox::Slot().FillWidth(1)[PlayerCard(1)]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,12)
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)[Label(TEXT("종족 선택"),17,Muted)]
                        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("에테리안"),[this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerSetRace(ESFRace::Human);})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[Button(TEXT("비스트"),[this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerSetRace(ESFRace::Beast);})]
                        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("랜덤"),[this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerSetRandomRace();})]
                        +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right)
                        [SNew(SBox).MinDesiredWidth(180)
                            [SNew(SFPVPUI::SForgeButton).ButtonStyle(&PrimaryButtonStyle).HAlign(HAlign_Center)
                                .OnClicked_Lambda([this]{auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer());auto* P=PC?PC->GetPlayerState<ASFPVPLobbyPlayerState>():nullptr;if(P)PC->ServerSetReady(!P->bReady);return FReply::Handled();})
                                [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).ColorAndOpacity(Cream)
                                    .Text_Lambda([this]{const auto* P=GetOwningPlayer()->GetPlayerState<ASFPVPLobbyPlayerState>();return FText::FromString(P&&P->bReady?TEXT("준비 취소"):TEXT("준비 완료"));})]]]]
                    +SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Muted).AutoWrapText(true)
                        .Text_Lambda([this]
                        {
                            const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();
                            if(S&&!S->StartError.IsEmpty())return FText::FromString(S->StartError);
                            if(S&&S->bStarting)return FText::FromString(TEXT("무작위 전장으로 함께 이동하고 있습니다."));
                            if(S&&S->bAIEnabled)return FText::FromString(S->AreBothReady()?TEXT("AI가 준비되었습니다. 전투 시작을 눌러 주세요."):TEXT("AI와 연습합니다. 종족을 선택하고 준비 완료를 눌러 주세요."));
                            if(S&&(!S->GetSeat(0)||!S->GetSeat(1)))return FText::FromString(TEXT("도전자가 입장하면 준비 후 무작위 전장으로 출전합니다."));
                            return FText::FromString(S&&S->AreBothReady()?TEXT("두 플레이어가 준비되었습니다. 방장이 전투를 시작하세요."):TEXT("종족 또는 랜덤을 선택한 뒤 준비해 주세요."));
                        })]]]]
            +SOverlay::Slot()[SNew(STrim).FullFrame(true)]]
        +SVerticalBox::Slot().AutoHeight().Padding(2,16,2,0)
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("돌아가기"),[this]{Sessions()->Leave(!InRoom());})]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(24,0)
            [SNew(STextBlock).Font(FSlateFontInfo(Font,17)).ColorAndOpacity(Cream).AutoWrapText(true)
                .Text_Lambda([this]{return FText::FromString(Sessions()->GetStatus());})]
            +SHorizontalBox::Slot().AutoWidth()
            [SNew(SBox).MinDesiredWidth(204)
            [SNew(SFPVPUI::SForgeButton).ButtonStyle(&PrimaryButtonStyle).HAlign(HAlign_Center)
                .Visibility_Lambda([this]{const auto* PC=GetOwningPlayer();const auto* P=PC?PC->GetPlayerState<ASFPVPLobbyPlayerState>():nullptr;return InRoom()&&P&&P->Seat==0?EVisibility::Visible:EVisibility::Collapsed;})
                .IsEnabled_Lambda([this]{const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();return S&&S->AreBothReady()&&!S->bStarting;})
                .OnClicked_Lambda([this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerStartBattle();return FReply::Handled();})
                [Label(TEXT("전투 시작  >"),23,Cream)]]]]
    ]]];
}

void USFPVPLobbyWidget::RefreshRooms()
{
    if(!RoomList)return;
    const auto* S=Sessions(); FString Signature=S->GetStatus();
    for(int32 I=0;I<S->GetRooms().Num();++I)Signature+=S->DescribeRoom(I);
    if(Signature==RoomSignature)return;
    RoomSignature=Signature; RoomList->ClearChildren();
    for(int32 I=0;I<S->GetRooms().Num();++I)
        RoomList->AddSlot().AutoHeight().Padding(0,0,0,8)
        [SNew(SFPVPUI::SForgeButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Fill)
            .IsEnabled_Lambda([this,I]{const auto* Sub=Sessions();return !Sub->IsBusy()&&Sub->GetRooms().IsValidIndex(I)&&Sub->GetRooms()[I].Session.NumOpenPublicConnections>0;})
            .OnClicked_Lambda([this,I]{Sessions()->JoinRoom(I);return FReply::Handled();})
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).ColorAndOpacity(SFPVPUI::Cream)
                        .Text(FText::FromString(S->DescribeRoom(I))).AutoWrapText(true)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)[Label(TEXT("참가  >"),18,SFPVPUI::Teal)]]];
}

void USFPVPLobbyWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if(Ambience)Ambience->Advance(DeltaTime);
    if(!InRoom()){RefreshRooms();return;}
    FString Signature;
    if(const auto* State=GetWorld()->GetGameState<ASFPVPLobbyState>())
    {
        Signature=FString::Printf(TEXT("%d:%s:%s:"),State->bStarting,*State->StartError,*State->RoomName);
        for(int32 Seat=0;Seat<2;++Seat)if(const auto* PS=State->GetSeat(Seat))
        {
            Signature+=FString::Printf(TEXT("%d:%d:%d:%d:%s;"),Seat,int32(PS->Race),PS->bRandomRace,PS->bReady,*PS->GetPlayerName());
            for (const ESFRace Race : {ESFRace::Human,ESFRace::Beast})
            {
                if ((!PS->bRandomRace && PS->Race!=Race) || WarmedRaces.Contains(uint8(Race)))continue;
                WarmedRaces.Add(uint8(Race));
                auto* Assets=GetGameInstance()->GetSubsystem<USFBuildingAssetSubsystem>();
                Assets->RequestRaceDataAsync(Race,FSFOnRaceBuildingDataReady::CreateWeakLambda(Assets,
                    [Assets](ESFRace LoadedRace,bool bOK)
                    {
                        auto* Catalog=Assets->GetLoadedCatalog(LoadedRace);if(!bOK||!Catalog)return;
                        TArray<FSoftObjectPath> Paths;Catalog->GetDefinitionPaths(Paths);
                        for(const auto& Path:Paths)if(auto* Def=Cast<USFBuildingDefinition>(Path.ResolveObject()))
                            Assets->RequestBuildingAssetsAsync(Def,0,FSFOnBuildingAssetsReady());
                    }));
            }
        }
    }
    if(const auto* PC=GetOwningPlayer())if(const auto* LocalState=PC->GetPlayerState<ASFPVPLobbyPlayerState>())Signature+=FString::FromInt(LocalState->Seat);
    if(Signature!=PlayerSignature)
    {
        PlayerSignature=Signature;
        // Replicated player state changes must invalidate the noninteractive card subtree as well.
        InvalidateLayoutAndVolatility(); ForceLayoutPrepass();
    }
}

void USFPVPLobbyWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RoomInput.Reset(); AddressInput.Reset(); RoomList.Reset(); Ambience.Reset(); RoomSignature.Empty(); PlayerSignature.Empty();
}
