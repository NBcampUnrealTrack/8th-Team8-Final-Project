#include "SFStoryWidget.h"
#include "SFStoryIllustrationActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Types/NavigationMetaData.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

namespace SFStory
{
    constexpr float OpenDuration = 1.15f;
    constexpr float CloseDuration = .55f;
    constexpr int32 ChapterCount = 5;
    const FLinearColor Ink(.115f, .20f, .18f);
    const FLinearColor Teal(.018f, .19f, .18f);
    const FLinearColor Gold(.65f, .43f, .17f);
    const FLinearColor Cream(.98f, .89f, .67f);

    struct FChapter { const TCHAR* Title; const TCHAR* Subtitle; const TCHAR* Body; const TCHAR* Quote; };
    const FChapter Chapters[] = {
        { TEXT("이름 없는 행성"), TEXT("별들 사이에서 시작된 이야기"),
          TEXT("거대한 우주 어딘가, 이름 없는 행성 하나가 태어났다.\n\n그곳의 생명은 다른 행성과 비교할 수 없을 만큼 빠르게 자라났고, 문명 또한 놀라운 속도로 발전했다.\n\n이 모든 변화의 중심에는 푸른빛을 품은 신비한 광물, 에테르가 있었다."),
          TEXT("모든 것은 푸른 빛에서 시작되었다.") },
        { TEXT("푸른 광물, 에테르"), TEXT("번영을 이끈 힘, 갈등의 씨앗"),
          TEXT("에테르의 힘은 생명의 성장과 문명의 발전을 이끌었다. 이 행성에서 살아가는 모두에게 에테르는 반드시 필요한 자원이었다.\n\n그러나 누구나 원하는 만큼 가질 수는 없었다. 푸른 광물이 있는 땅을 차지하려는 경쟁은 점점 치열해졌다.\n\n번영을 가져온 빛은, 어느새 분쟁의 불씨가 되었다."),
          TEXT("같은 빛을 원했지만, 나눌 땅은 부족했다.") },
        { TEXT("비스트의 탄생"), TEXT("깨어난 지성, 새로운 세력"),
          TEXT("어느 날, 알 수 없는 이유로 동물들의 유전자가 변하기 시작했다. 그 변화 속에서 본래 지니지 못했던 지성을 얻은 생명체들이 탄생했다.\n\n그들은 스스로를 비스트라 불렀다. 비스트는 서로 힘을 모아 세력을 키우고, 여러 땅과 나라를 다스리기 시작했다.\n\n하지만 늘어나는 세력에 비해, 그들이 가진 에테르는 턱없이 부족했다."),
          TEXT("깨어난 자들은 자신들의 땅을 원했다.") },
        { TEXT("에테리안의 영역"), TEXT("고향과 푸른 보석의 수호자"),
          TEXT("에테리안은 이 행성의 원주민이자, 에테르의 힘을 자유자재로 다루는 종족이었다. 그들은 행성에 존재하는 에테르의 절반 이상을 차지하고 관리했다.\n\n갑작스레 나타난 비스트를 적대시한 에테리안은, 그들이 에테르에 접근하지 못하도록 막았다.\n\n에테리안에게 그 땅은 오랜 고향이었고, 지켜야 할 힘의 근원이었다."),
          TEXT("수호자들은 자신들의 고향을 지키려 했다.") },
        { TEXT("전쟁의 서막"), TEXT("하나의 행성, 두 종족의 운명"),
          TEXT("부족한 에테르를 확보하기 위해 비스트는 에테리안이 점령한 풍요로운 땅으로 향했다. 에테르가 풍부한 지역을 차지하려는 전쟁이 곳곳에서 일어났다.\n\n에테리안 역시 고향과 에테르를 지키기 위해 비스트를 제거할 계획을 세우기 시작했다.\n\n같은 행성에서 살아가는 두 종족. 푸른빛을 둘러싼 싸움은 이제, 당신이 펼칠 전장의 이야기로 이어진다."),
          TEXT("이제, 다음 이야기는 전장에서 쓰인다.") }
    };

    float Ease(float Value)
    {
        const float T = FMath::Clamp(Value, 0.f, 1.f);
        return T * T * (3.f - 2.f * T);
    }
}

/** The illustration is vector art: faceted crystals, aged paper and wood/gold scroll rods. */
class SSFStoryScroll : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSFStoryScroll) {} SLATE_END_ARGS()
    void Construct(const FArguments&, USFStoryWidget* InOwner, UFont* Font)
    {
        Owner = InOwner;
        auto Typeface = [Font](int32 Size)
        {
            return Font ? FSlateFontInfo(Font, Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
        };
        auto Button = [&](const FText& Label, TFunction<FReply()> Action)
        {
            return SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "Button")
                .ButtonColorAndOpacity(SFStory::Teal)
                .ContentPadding(FMargin(24, 10))
                .OnClicked_Lambda(MoveTemp(Action))
                [ SNew(STextBlock).Text(Label).Font(Typeface(22)).ColorAndOpacity(SFStory::Cream) ];
        };
        ChildSlot
        [
            SAssignNew(Content, SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(154, 120, 154, 0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [ SNew(STextBlock).Text(FText::FromString(TEXT("에테르의 연대기"))).Font(Typeface(22)).ColorAndOpacity(SFStory::Gold) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ Button(FText::FromString(TEXT("닫기")), [this] { if (Owner.IsValid()) Owner->CloseStory(); return FReply::Handled(); }) ]
            ]
            + SVerticalBox::Slot().FillHeight(1).Padding(154, 35, 154, 36)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(SBox).WidthOverride(390) ]
                + SHorizontalBox::Slot().FillWidth(1).Padding(40, 4, 0, 0)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ SAssignNew(ChapterLabel, STextBlock).Font(Typeface(20)).ColorAndOpacity(SFStory::Gold) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 8)
                    [ SAssignNew(Title, STextBlock).Font(Typeface(44)).ColorAndOpacity(SFStory::Ink) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
                    [ SAssignNew(Subtitle, STextBlock).Font(Typeface(22)).ColorAndOpacity(SFStory::Gold) ]
                    + SVerticalBox::Slot().FillHeight(1)
                    [
                        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                            .HAlign(HAlign_Left).VAlign(VAlign_Top)
                        [ SNew(SBox).WidthOverride(650)
                            [ SAssignNew(Body, STextBlock).Font(Typeface(20)).ColorAndOpacity(SFStory::Ink)
                                .WrapTextAt(650).LineHeightPercentage(1.f) ] ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(154, 0, 154, 116)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [ SAssignNew(Previous, SButton).ButtonColorAndOpacity(SFStory::Teal).ContentPadding(FMargin(24, 10))
                    .OnClicked_Lambda([this] { if (Owner.IsValid()) Owner->TurnPage(-1); return FReply::Handled(); })
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("이전"))).Font(Typeface(22)).ColorAndOpacity(SFStory::Cream) ] ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)
                [ SAssignNew(PageLabel, STextBlock).Font(Typeface(20)).ColorAndOpacity(SFStory::Ink) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ SAssignNew(Next, SButton).ButtonColorAndOpacity(SFStory::Teal).ContentPadding(FMargin(24, 10))
                    .OnClicked_Lambda([this] { if (Owner.IsValid()) Owner->TurnPage(1); return FReply::Handled(); })
                    [ SAssignNew(NextLabel, STextBlock).Font(Typeface(22)).ColorAndOpacity(SFStory::Cream) ] ]
            ]
        ];
        // Keep Tab navigation inside the chronicle while it covers the lobby.
        TSharedRef<FNavigationMetaData> Navigation = MakeShared<FNavigationMetaData>();
        Navigation->SetNavigationWrap(EUINavigation::Next);
        Navigation->SetNavigationWrap(EUINavigation::Previous);
        AddMetadata(Navigation);
        QuoteFont = Typeface(20);
    }

    void Present(int32 Index, float InAmount, float InAlpha, float InPageAlpha, float InAge, float InPageAge, UMaterialInterface* InCapture)
    {
        Amount = InAmount; Alpha = InAlpha; PageAlpha = InPageAlpha;
        Age=InAge; PageAge=InPageAge;
        CaptureBrush.SetResourceObject(InCapture);
        CaptureBrush.DrawAs=ESlateBrushDrawType::Image;
        if (Chapter != Index)
        {
            Chapter = Index;
            const auto& Data = SFStory::Chapters[Chapter];
            Title->SetText(FText::FromString(Data.Title));
            Subtitle->SetText(FText::FromString(Data.Subtitle));
            Body->SetText(FText::FromString(Data.Body));
            ChapterLabel->SetText(FText::FromString(FString::Printf(TEXT("CHAPTER  %02d"), Chapter + 1)));
            PageLabel->SetText(FText::FromString(FString::Printf(TEXT("%02d  /  05       방향키: 넘기기     ESC: 닫기"), Chapter + 1)));
            NextLabel->SetText(FText::FromString(Chapter == 4 ? TEXT("이야기 접기") : TEXT("다음")));
        }
        Previous->SetEnabled(Chapter > 0 && Amount >= .99f);
        Next->SetEnabled(Amount >= .99f);
        Content->SetRenderOpacity(PageAlpha * Alpha);
        Content->SetVisibility(PageAlpha > .01f ? EVisibility::Visible : EVisibility::Hidden);
        Content->SetEnabled(Amount >= .99f);
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
    {
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
        const FLinearColor Tint = Style.GetColorAndOpacityTint();
        auto Box = [&](FVector2D P, FVector2D Size, FLinearColor C, int32 Z = 0)
        {
            C.A *= Alpha;
            FSlateDrawElement::MakeBox(Out, Layer + Z, G.ToPaintGeometry(Size, FSlateLayoutTransform(P)), White, ESlateDrawEffect::None, C * Tint);
        };
        auto Polygon = [&](const TArray<FVector2D>& Points, FLinearColor C, int32 Z = 1)
        {
            C.A *= Alpha;
            TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
            for (const FVector2D& P : Points)
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f(.5f,.5f), (C * Tint).ToFColor(true)));
            for (int32 I = 1; I + 1 < Points.Num(); ++I) Indices.Append({0, static_cast<SlateIndex>(I), static_cast<SlateIndex>(I+1)});
            FSlateDrawElement::MakeCustomVerts(Out, Layer + Z, Resource, Vertices, Indices, nullptr, 0, 0);
        };
        auto Line = [&](const TArray<FVector2D>& Points, FLinearColor C, float Width = 1.f, int32 Z = 2)
        {
            C.A *= Alpha;
            FSlateDrawElement::MakeLines(Out, Layer + Z, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C * Tint, true, Width);
        };
        auto Ring = [&](FVector2D Center, FVector2D Radius, FLinearColor C, float Width = 1.f, int32 Z = 3)
        {
            TArray<FVector2D> Points;
            for (int32 I=0; I<=64; ++I) { const float A=2*PI*I/64; Points.Add(Center+FVector2D(FMath::Cos(A)*Radius.X,FMath::Sin(A)*Radius.Y)); }
            Line(Points,C,Width,Z);
        };
        auto Diamond = [&](FVector2D P, float Size, FLinearColor C, int32 Z = 3)
        { Polygon({P+FVector2D(0,-Size),P+FVector2D(Size*.65f,0),P+FVector2D(0,Size),P-FVector2D(Size*.65f,0)},C,Z); };

        // Both rollers start together, then carry the paper outwards.
        const float Left = FMath::Lerp(686.f, 78.f, Amount), Right = 1440.f - Left;
        const float Lift = -12.f * FMath::Sin(Amount * PI);
        Box({Left-18, 98+Lift}, {Right-Left+36, 720}, FLinearColor(0,0,0,.18f));
        Polygon({{Left,96+Lift},{Right,96+Lift},{Right,794+Lift},{Left,794+Lift}}, FLinearColor(.83f,.64f,.36f),1);
        Polygon({{Left+8,103+Lift},{Right-8,105+Lift},{Right-8,785+Lift},{Left+8,787+Lift}}, FLinearColor(.96f,.86f,.64f),2);
        // Broad translucent strips give the paper a warm edge and a pale reading area.
        for (int32 I=0; I<18; ++I)
        {
            const float T=I/18.f;
            Box({Left+8+(Right-Left-16)*T,109+Lift},{(Right-Left-16)/18.f+1,670},FLinearColor(1,.95f,.79f,.12f*FMath::Sin(T*PI)),3);
        }
        // Deterministic fibres, sparse enough to stay behind the lettering.
        for (int32 I=0; I<75; ++I)
        {
            const float X=Left+20+FMath::Frac(I*.618034f)*FMath::Max(0.f,Right-Left-40);
            const float Y=116+FMath::Frac(I*.381966f)*650+Lift;
            Line({{X,Y},{X+FMath::Min(12.f,Right-X-10),Y-.5f}},FLinearColor(.42f,.29f,.13f,.075f),.7f,4);
        }
        if (Amount > .35f)
        {
            Line({{Left+30,126+Lift},{Left+46,112+Lift},{Right-46,112+Lift},{Right-30,126+Lift},{Right-30,762+Lift},{Right-46,776+Lift},{Left+46,776+Lift},{Left+30,762+Lift},{Left+30,126+Lift}},FLinearColor(.50f,.34f,.16f,.6f),2.f,5);
            Line({{Left+39,137+Lift},{Left+52,121+Lift},{Right-52,121+Lift},{Right-39,137+Lift}},FLinearColor(.67f,.48f,.24f,.4f),1.f,5);
        }
        for (float X : {Left,Right})
        {
            Box({X-23,98+Lift},{46,697},FLinearColor(.63f,.43f,.23f),6);
            Box({X-16,98+Lift},{32,697},FLinearColor(.84f,.65f,.37f),7);
            Box({X-8,98+Lift},{11,697},FLinearColor(.96f,.80f,.49f),8);
            Box({X+12,98+Lift},{6,697},FLinearColor(.40f,.26f,.12f,.35f),8);
            for (float Y : {66.f,784.f})
            {
                Polygon({{X-22,Y+Lift},{X+22,Y+Lift},{X+16,Y+50+Lift},{X-16,Y+50+Lift}},FLinearColor(.24f,.13f,.067f),9);
                Box({X-13,Y-14+Lift},{26,18},FLinearColor(.35f,.20f,.095f),9);
                Box({X-29,Y+28+Lift},{58,12},SFStory::Gold,10);
                Box({X-25,Y+28+Lift},{50,3},SFStory::Cream,11);
                Diamond({X,Y+6+Lift},10,FLinearColor(.08f,.68f,.70f),11);
            }
        }

        if (PageAlpha > .01f && Chapter >= 0)
        {
            const float A = PageAlpha;
            FLinearColor FaintGold = SFStory::Gold; FaintGold.A = .35f*A;
            Line({{154,190},{1286,190}},FaintGold,1.f,12);
            Line({{154,705},{1286,705}},FaintGold,1.f,12);
            Line({{568,243},{568,661}},FaintGold,1.f,12);
            const FVector2D Center(347,424);
            const float BackgroundReveal=SFStory::Ease(PageAge/.48f);
            const float MidReveal=SFStory::Ease((PageAge-.14f)/.55f);
            const float ForegroundReveal=SFStory::Ease((PageAge-.30f)/.62f);
            const FVector2D Far(FMath::Sin(Age*.23f)*2.f,FMath::Cos(Age*.19f)*1.5f);
            const FVector2D Mid(FMath::Sin(Age*.39f)*4.f,(1-MidReveal)*14+FMath::Cos(Age*.32f)*2.f);
            const FVector2D Near(FMath::Sin(Age*.55f)*6.f,(1-ForegroundReveal)*20+FMath::Sin(Age*.8f)*3.f);
            const bool bWarm=Chapter==2;
            const FLinearColor Sky=bWarm ? FLinearColor(.15f,.095f,.055f,A*BackgroundReveal) : FLinearColor(.025f,.12f,.145f,A*BackgroundReveal);
            TArray<FVector2D> Backdrop;
            for(int32 I=0;I<64;++I) { float R=2*PI*I/64; Backdrop.Add(Center+FVector2D(FMath::Cos(R),FMath::Sin(R))*160); }
            Polygon(Backdrop,Sky,13);
            Ring(Center,{169,169},FaintGold,2.f,14);
            Ring(Center,{163,163},FLinearColor(.72f,.54f,.25f,.45f*A),1.f,14);
            for(int32 I=0;I<12;++I)
            {
                const float Angle=I*PI/6+Age*.035f;
                Diamond(Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*169,I%3==0?5:2.5f,FaintGold,15);
            }
            auto Image = [&](int32 Index,FVector2D P,FVector2D Size,float Opacity,int32 Z)
            {
                UTexture2D* Texture=Owner.IsValid()?Owner->GetIllustrationTexture(Index):nullptr;
                if(!Texture) return;
                FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.DrawAs=ESlateBrushDrawType::Image;
                FSlateDrawElement::MakeBox(Out,Layer+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),&Brush,ESlateDrawEffect::None,FLinearColor(1,1,1,Opacity*Alpha)*Tint);
            };
            // Layer 1: distant stars/landscape. It moves least and arrives first.
            if(Chapter==0)
            {
                for(int32 I=0;I<24;++I)
                {
                    const float Angle=I*2.39996f;
                    const float Radius=45+FMath::Frac(I*.381966f)*103;
                    const float Pulse=.45f+.55f*FMath::Square(FMath::Sin(Age*.8f+I));
                    Diamond(Center+Far+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius,1.5f+Pulse*2,FLinearColor(.65f,.85f,.8f,A*BackgroundReveal*Pulse),15);
                }
            }
            else
            {
                Polygon({FVector2D(202,473)+Far,FVector2D(238,394)+Far,FVector2D(281,448)+Far,FVector2D(335,367)+Far,FVector2D(402,441)+Far,FVector2D(472,407)+Far,FVector2D(494,482)+Far,FVector2D(454,523),FVector2D(243,523)},FLinearColor(.17f,.30f,.24f,A*BackgroundReveal),15);
                Polygon({FVector2D(230,493)+Far,FVector2D(273,449)+Far,FVector2D(324,482)+Far,FVector2D(401,432)+Far,FVector2D(471,495)+Far,FVector2D(442,550),FVector2D(282,552)},FLinearColor(.11f,.22f,.17f,A*BackgroundReveal),16);
                for(int32 I=0;I<7;++I)
                {
                    const FVector2D P=FVector2D(234+I*34.f,465+FMath::Sin(I*1.9f)*28)+Far;
                    Polygon({P+FVector2D(0,-32),P+FVector2D(15,6),P+FVector2D(-15,6)},FLinearColor(.06f,.24f,.20f,A*BackgroundReveal),17);
                }
            }
            // Layer 2: planet, ether deposits, and the game's original faction buildings.
            if(Chapter==0)
            {
                const FVector2D PlanetCenter=Center+Mid;
                TArray<FVector2D> Planet;
                for(int32 I=0;I<12;++I) {float Angle=I*PI/6;Planet.Add(PlanetCenter+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*98);}
                Polygon(Planet,FLinearColor(.04f,.32f,.36f,A*MidReveal),18);
                Polygon({PlanetCenter+FVector2D(-84,-49),PlanetCenter+FVector2D(0,-98),PlanetCenter+FVector2D(85,-49),PlanetCenter+FVector2D(37,12),PlanetCenter+FVector2D(-35,-3)},FLinearColor(.24f,.53f,.42f,A*MidReveal),19);
                Polygon({PlanetCenter+FVector2D(-84,-49),PlanetCenter+FVector2D(-35,-3),PlanetCenter+FVector2D(-49,70),PlanetCenter+FVector2D(-84,49),PlanetCenter+FVector2D(-98,0)},FLinearColor(.40f,.62f,.43f,A*MidReveal),19);
                Ring(PlanetCenter,{126,40+FMath::Sin(Age*.45f)*9},FLinearColor(.63f,.84f,.68f,.7f*A*MidReveal),2,20);
                Image(2,Center+Near-FVector2D(45,40),{90,90},A*ForegroundReveal,21);
            }
            else if(Chapter==1)
            {
                Image(2,FVector2D(204,374)+Mid,{132,132},A*MidReveal*.7f,18);
                Image(2,FVector2D(370,371)-Mid*.6f,{118,118},A*MidReveal*.75f,18);
                Ring(Center+FVector2D(0,92),{124,24},FLinearColor(.1f,.78f,.8f,A*MidReveal*(.4f+.15f*FMath::Sin(Age*1.4f))),2,19);
                Image(2,FVector2D(236,308)+Near,{230,230},A*ForegroundReveal,20);
            }
            else if(Chapter==2 || Chapter==3)
            {
                Image(Chapter==2?0:1,FVector2D(198,345)+Mid,{215,215},A*MidReveal,18);
                Image(2,FVector2D(387,479)-Mid*.6f,{84,84},A*MidReveal*.8f,19);
            }
            else
            {
                Image(0,FVector2D(190,357)+Mid,{156,156},A*MidReveal,18);
                Image(1,FVector2D(350,341)-Mid*.7f,{158,158},A*MidReveal,18);
                Image(2,FVector2D(300,462)+Near*.3f,{96,96},A*ForegroundReveal,19);
            }
            // Layer 3: real animated commander meshes and Niagara, captured into the UI.
            if(CaptureBrush.GetResourceObject())
                FSlateDrawElement::MakeBox(Out,Layer+22,G.ToPaintGeometry(FVector2D(360,360),FSlateLayoutTransform(Center-FVector2D(180,180)+Near)),&CaptureBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,A*ForegroundReveal*Alpha)*Tint);
            const FString Quote = SFStory::Chapters[Chapter].Quote;
            const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Quote, QuoteFont);
            FSlateDrawElement::MakeText(Out,Layer+23,G.ToPaintGeometry(FVector2D(400,40),FSlateLayoutTransform(FVector2D(347-TextSize.X/2,622))),Quote,QuoteFont,ESlateDrawEffect::None,FLinearColor(.43f,.33f,.18f,A*Alpha)*Tint);
            for(int32 I=0;I<5;++I) Diamond({299+I*24.f,678},I==Chapter?6:3,FLinearColor(.05f,.40f,.37f,(I==Chapter?1.f:.30f)*A),23);
        }
        return SCompoundWidget::OnPaint(Args,G,Cull,Out,Layer+24,Style,bEnabled);
    }
private:
    TWeakObjectPtr<USFStoryWidget> Owner;
    TSharedPtr<SVerticalBox> Content;
    TSharedPtr<STextBlock> ChapterLabel, Title, Subtitle, Body, PageLabel, NextLabel;
    TSharedPtr<SButton> Previous, Next;
    FSlateFontInfo QuoteFont;
    int32 Chapter = INDEX_NONE;
    float Amount=0, Alpha=0, PageAlpha=0;
    float Age=0, PageAge=0;
    FSlateBrush CaptureBrush;
};

USFStoryWidget::USFStoryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    StoryFont = Font.Object;
    const TCHAR* Paths[] = {
        TEXT("/Game/Game/IN_UI/Image/Building/BeastBuilding/T_SFBeastNexus1T.T_SFBeastNexus1T"),
        TEXT("/Game/Game/IN_UI/Image/Building/EtherianBuilding/T_SFHumanNexus3T.T_SFHumanNexus3T"),
        TEXT("/Game/Game/IN_UI/Image/Building/Resource/T_SFEther.T_SFEther") };
    for (const TCHAR* Path : Paths)
    {
        ConstructorHelpers::FObjectFinder<UTexture2D> Art(Path);
        IllustrationTextures.Add(Art.Object);
    }
}

TSharedRef<SWidget> USFStoryWidget::RebuildWidget()
{
    // A hard asset reference keeps the existing Korean game font in packaged builds.
    return SNew(SOverlay)
        + SOverlay::Slot()
        [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([this] { return FLinearColor(.005f,.025f,.025f,.70f*(bClosing?1-SFStory::Ease(ClosingElapsed/SFStory::CloseDuration):SFStory::Ease(OpeningElapsed/.3f))); })
            .OnMouseButtonDown_Lambda([](const FGeometry&,const FPointerEvent&) { return FReply::Handled(); }) ]
        + SOverlay::Slot()
        [ SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(1440).HeightOverride(900)
                [ SAssignNew(Scroll,SSFStoryScroll,this,StoryFont.Get()) ] ] ];
}

void USFStoryWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Scroll.Reset();
}

void USFStoryWidget::OpenStory()
{
    ChapterIndex=0; OpeningElapsed=0; PageElapsed=0; ClosingElapsed=0;
    bClosing=false; bPreview=false;
    IllustrationElapsed=0.f;
    DestroyIllustration();
    if (GetWorld())
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        IllustrationStage=GetWorld()->SpawnActor<ASFStoryIllustrationActor>(FVector(0,0,-200000),FRotator::ZeroRotator,Params);
        if (IllustrationStage) IllustrationStage->SetChapter(0);
    }
    SetVisibility(ESlateVisibility::Visible);
    RefreshPresentation();
    SetFocus();
}

void USFStoryWidget::CloseStory()
{
    if (bClosing) return;
    CloseStartAmount=SFStory::Ease((OpeningElapsed-.12f)/SFStory::OpenDuration);
    ClosingElapsed=0; bClosing=true; bPreview=false;
    RefreshPresentation();
}

void USFStoryWidget::TurnPage(int32 Direction)
{
    if (bClosing || OpeningElapsed < SFStory::OpenDuration+.12f) return;
    if (Direction > 0 && ChapterIndex == SFStory::ChapterCount-1) { CloseStory(); return; }
    const int32 NewIndex=FMath::Clamp(ChapterIndex+Direction,0,SFStory::ChapterCount-1);
    if (NewIndex == ChapterIndex) return;
    ChapterIndex=NewIndex; PageElapsed=0; bPreview=false;
    if (IllustrationStage) IllustrationStage->SetChapter(NewIndex);
    RefreshPresentation();
}

void USFStoryWidget::RefreshPresentation()
{
    if (!Scroll) return;
    const float Amount=bClosing ? CloseStartAmount*(1-SFStory::Ease(ClosingElapsed/SFStory::CloseDuration)) : SFStory::Ease((OpeningElapsed-.12f)/SFStory::OpenDuration);
    const float Alpha=bClosing ? 1-SFStory::Ease((ClosingElapsed-.38f)/.17f) : SFStory::Ease(OpeningElapsed/.2f);
    const float PageAlpha=bClosing ? 1-SFStory::Ease(ClosingElapsed/.16f) : SFStory::Ease((OpeningElapsed-1.05f)/.35f)*SFStory::Ease(PageElapsed/.24f);
    Scroll->Present(ChapterIndex,Amount,Alpha,PageAlpha,IllustrationElapsed,PageElapsed,
        IllustrationStage ? IllustrationStage->GetPresentationMaterial() : nullptr);
}

void USFStoryWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if (bPreview) return;
    OpeningElapsed+=DeltaTime; PageElapsed+=DeltaTime;
    IllustrationElapsed+=DeltaTime;
    if (bClosing)
    {
        ClosingElapsed+=DeltaTime;
        if (ClosingElapsed>=SFStory::CloseDuration)
        {
            SetVisibility(ESlateVisibility::Collapsed);
            DestroyIllustration();
            OnClosed.Broadcast();
            return;
        }
    }
    RefreshPresentation();
}

UTexture2D* USFStoryWidget::GetIllustrationTexture(int32 Index) const
{
    return IllustrationTextures.IsValidIndex(Index) ? IllustrationTextures[Index].Get() : nullptr;
}

bool USFStoryWidget::HasLiveIllustration() const
{
    return IsValid(IllustrationStage) && IllustrationStage->HasActiveParticles();
}

void USFStoryWidget::DestroyIllustration()
{
    if (IllustrationStage) IllustrationStage->Destroy();
    IllustrationStage=nullptr;
}

void USFStoryWidget::NativeDestruct()
{
    DestroyIllustration();
    Super::NativeDestruct();
}

FReply USFStoryWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if (Key==EKeys::Escape) { CloseStory(); return FReply::Handled(); }
    if (Key==EKeys::Left || Key==EKeys::PageUp) { TurnPage(-1); return FReply::Handled(); }
    if (Key==EKeys::Right || Key==EKeys::PageDown) { TurnPage(1); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}

#if WITH_DEV_AUTOMATION_TESTS
void USFStoryWidget::PreviewChapter(int32 Index,float OpeningTime)
{
    ChapterIndex=FMath::Clamp(Index,0,SFStory::ChapterCount-1);
    OpeningElapsed=OpeningTime; PageElapsed=2.f; bPreview=true; bClosing=false;
    if (IllustrationStage) IllustrationStage->SetChapter(ChapterIndex);
    RefreshPresentation();
}
#endif
