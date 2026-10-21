#include "SFAchievementToastWidget.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Styling/CoreStyle.h"

USFAchievementToastWidget::USFAchievementToastWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetIsFocusable(false);
    static ConstructorHelpers::FObjectFinder<UFont> Heading(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    static ConstructorHelpers::FObjectFinder<UFontFace> Body(TEXT("/Game/Game/IN_UI/Font/NotoSansKR_Medium.NotoSansKR_Medium"));
    TitleFont = Heading.Object;
    BodyFace = Body.Object;
}

void USFAchievementToastWidget::InitializeAchievement(const FSFPVEAchievement& Entry)
{
    Achievement = Entry;
    if (!BodyFont && BodyFace)
    {
        BodyFont = NewObject<UFont>(this);
        BodyFont->FontCacheType = EFontCacheType::Runtime;
        FTypefaceEntry FaceEntry(TEXT("Default"));
        FaceEntry.Font = FFontData(BodyFace);
        BodyFont->CompositeFont.DefaultTypeface.Fonts.Add(FaceEntry);
    }
    StartTime = FSlateApplication::Get().GetCurrentTime();
}

float USFAchievementToastWidget::GetAge() const
{
    return PreviewTime >= 0 ? PreviewTime : float(FSlateApplication::Get().GetCurrentTime() - StartTime) - Delay;
}

void USFAchievementToastWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (GetAge() >= Duration && PreviewTime < 0) RemoveFromParent();
}

int32 USFAchievementToastWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
    FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    Layer = Super::NativePaint(Args,G,Cull,Out,Layer,Style,bEnabled) + 1;
    const float Age = GetAge();
    if (Age < 0 || Age >= Duration) return Layer;
    const float Arrive = FMath::SmoothStep(0.f,.48f,Age);
    const float Alpha = Arrive * (1.f - FMath::SmoothStep(Duration-.65f,Duration,Age));
    const float Scale = FMath::Min(G.GetLocalSize().X/1920.f,G.GetLocalSize().Y/1080.f) * FMath::Lerp(.88f,1.f,Arrive);
    const FVector2D Origin = G.GetLocalSize()*.5 - FVector2D(480,170)*Scale + FVector2D(0,20*(1-Arrive)*Scale);
    const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("DDBB73")));
    const FLinearColor Cream = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFF0CB")));
    const FLinearColor Aqua = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("65F5DC")));
    const FLinearColor Teal(.012f,.075f,.07f,.98f);
    auto Tint = [&](FLinearColor C) { C.A *= Alpha; return C * Style.GetColorAndOpacityTint(); };
    auto Point = [&](FVector2D P) { return Origin + P*Scale; };
    const auto* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
    const auto* Proxy = Resource.GetResourceProxy();
    const FVector2f UV = Proxy ? Proxy->StartUV + Proxy->SizeUV*.5f : FVector2f(.5f,.5f);
    auto Polygon = [&](const TArray<FVector2D>& Points, FLinearColor C)
    {
        TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
        for (const auto& P : Points)
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(Point(P)), UV, Tint(C).ToFColor(true)));
        for (int32 I=1; I+1<Points.Num(); ++I) Indices.Append({0,SlateIndex(I),SlateIndex(I+1)});
        FSlateDrawElement::MakeCustomVerts(Out,Layer++,Resource,Vertices,Indices,nullptr,0,0);
    };
    auto Line = [&](TArray<FVector2D> Points, FLinearColor C, float Width = 2.f)
    {
        for (auto& P : Points) P = Point(P);
        FSlateDrawElement::MakeLines(Out,Layer++,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Tint(C),true,Width*Scale);
    };
    auto Gem = [&](FVector2D P, float R, FLinearColor C)
    {
        Polygon({P+FVector2D(0,-R),P+FVector2D(R*.65f,0),P+FVector2D(0,R),P-FVector2D(R*.65f,0)},C);
        Polygon({P+FVector2D(0,-R),P,P-FVector2D(R*.65f,0)},Cream.CopyWithNewOpacity(.85f));
    };
    auto Text = [&](const FText& Label, float Y, int32 Size, FLinearColor C, UFont* Font)
    {
        const FSlateFontInfo Info(Font,Size);
        const FVector2D Extent = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label,Info);
        const FVector2D P = Point(FVector2D(480-Extent.X*.5,Y));
        FSlateDrawElement::MakeText(Out,Layer++,G.ToPaintGeometry(Extent,FSlateLayoutTransform(Scale,P+FVector2D(0,2*Scale))),Label,Info,ESlateDrawEffect::None,Tint(FLinearColor(0,0,0,.8f)));
        FSlateDrawElement::MakeText(Out,Layer++,G.ToPaintGeometry(Extent,FSlateLayoutTransform(Scale,P)),Label,Info,ESlateDrawEffect::None,Tint(C));
    };
    // Soft, local halo leaves the surrounding victory scene visible.
    for (int32 I=12; I>0; --I)
    {
        TArray<FVector2D> Ring;
        for (int32 K=0; K<48; ++K)
        {
            const float A=2*PI*K/48;
            Ring.Add(FVector2D(480+FMath::Cos(A)*(110+I*14),140+FMath::Sin(A)*(90+I*9)));
        }
        Polygon(Ring,FLinearColor(.02f,.18f,.14f,.018f));
    }
    // Faceted laurel leaves echo the lobby's low-poly artwork.
    for (float Side : {-1.f,1.f})
    {
        TArray<FVector2D> Stem;
        for (int32 I=0; I<=12; ++I)
        {
            const float A=I/12.f*1.6f;
            Stem.Add(FVector2D(480+Side*(32+92*FMath::Sin(A)),174-130*(1-FMath::Cos(A))));
        }
        Line(Stem,Gold,3.f);
        for (int32 I=1; I<=8; ++I)
        {
            const float A=I/9.f*1.6f;
            const FVector2D P(480+Side*(32+92*FMath::Sin(A)),174-130*(1-FMath::Cos(A)));
            Polygon({P,P+FVector2D(Side*30,-24),P+FVector2D(Side*34,-8),P+FVector2D(Side*9,5)},Gold);
            Polygon({P,P+FVector2D(-Side*20,-24),P+FVector2D(-Side*18,-5)},Cream.CopyWithNewOpacity(.85f));
        }
    }
    // Trophy silhouette and ether jewel.
    Polygon({{437,44},{523,44},{517,91},{500,114},{480,120},{460,114},{443,91}},Gold);
    Polygon({{474,116},{486,116},{486,144},{474,144}},Gold);
    Polygon({{474,140},{486,140},{510,148},{510,158},{450,158},{450,148}},Gold);
    Polygon({{445,51},{515,51},{510,88},{495,106},{480,112},{465,106},{450,88}},Teal);
    Line({{439,56},{421,56},{424,82},{447,98}},Gold,6.f);
    Line({{521,56},{539,56},{536,82},{513,98}},Gold,6.f);
    Gem({480,78},24,Aqua);
    // Gold-edged ribbon with clipped corners.
    Polygon({{85,181},{875,181},{897,203},{897,285},{875,307},{85,307},{63,285},{63,203}},FLinearColor(0,0,0,.5f));
    Polygon({{85,175},{875,175},{891,191},{891,281},{875,297},{85,297},{69,281},{69,191}},Gold);
    Polygon({{88,180},{872,180},{886,194},{886,278},{872,292},{88,292},{74,278},{74,194}},Teal);
    Line({{95,187},{865,187}},Cream.CopyWithNewOpacity(.35f),1.f);
    Line({{95,285},{865,285}},Gold.CopyWithNewOpacity(.45f),1.f);
    Gem({69,236},17,Aqua); Gem({891,236},17,Aqua);
    Text(NSLOCTEXT("SFAchievements","ToastHeader","업적 달성"),190,19,Gold,BodyFont);
    Text(Achievement.Title,218,31,Cream,TitleFont);
    Text(Achievement.Description,264,14,Aqua,BodyFont);
    for (int32 I=0; I<12; ++I)
    {
        const float A=I*2.39996f;
        const float Radius=110+FMath::Frac(I*.618f)*65+FMath::Min(Age,1.5f)*22;
        const FVector2D P(480+FMath::Cos(A)*Radius*1.4f,130+FMath::Sin(A)*Radius*.65f);
        const float Pulse=FMath::Max(0.f,FMath::Sin(Age*3.f+I));
        Line({P-FVector2D(5+Pulse*3,0),P+FVector2D(5+Pulse*3,0)},Cream.CopyWithNewOpacity(Pulse*.8f),1.f);
        Line({P-FVector2D(0,5+Pulse*3),P+FVector2D(0,5+Pulse*3)},Aqua.CopyWithNewOpacity(Pulse*.7f),1.f);
    }
    return Layer;
}
