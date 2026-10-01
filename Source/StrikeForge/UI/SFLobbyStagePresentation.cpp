#include "SFLobbyStagePresentation.h"
#include "SFMenuAtmosphereWidget.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Balanced return path, with the last fortress on the left of the parchment.
    const FVector2D Centers[] = { {.31,.40}, {.51,.40}, {.71,.40}, {.76,.65}, {.56,.67}, {.36,.67}, {.20,.57} };
    constexpr float RevealStart = 1.50f; // Rolls finish at 1.32; the landing settles by 1.48.
    constexpr float RevealStep = .055f;
    constexpr float RevealDuration = .25f;
    constexpr float LaunchDuration = .72f;
    float Reveal(float Time, int32 Index)
    {
        const float X = FMath::Clamp((Time-RevealStart-Index*RevealStep)/RevealDuration,0.f,1.f);
        return X*X*(3.f-2.f*X);
    }
}

USFLobbyStagePresentation::USFLobbyStagePresentation(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Icons(TEXT("/Game/Core/UI/LobbyAtmosphere/T_LobbyStageIcons.T_LobbyStageIcons"));
    IconAtlas = Icons.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> Ruins(TEXT("/Game/Core/UI/LobbyAtmosphere/T_LobbyClearedTower.T_LobbyClearedTower"));
    ClearedIcon = Ruins.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeaderArt(TEXT("/Game/Core/UI/LobbyAtmosphere/M_LobbyHeader.M_LobbyHeader"));
    HeaderMaterial = HeaderArt.Object;
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USFLobbyStagePresentation::InitializeControls(UUserWidget* Lobby, USFMenuAtmosphereWidget* Backdrop, const TArray<FName>& Rows)
{
    Atmosphere = Backdrop;
    const TCHAR* Names[] = { TEXT("Stage1"),TEXT("Stage2"),TEXT("Stage3"),TEXT("Stage4"),TEXT("Stage5"),TEXT("Stage6"),TEXT("Stage6_1") };
    USFGameInstance* GI = GetGameInstance<USFGameInstance>();
    for (int32 I=0; I<7; ++I)
    {
        FSFLobbyStageControl& State = Controls.AddDefaulted_GetRef();
        State.Button = Cast<UButton>(Lobby->GetWidgetFromName(Names[I]));
        if (!State.Button) continue;
        State.OriginalClick = State.Button->OnClicked;
        State.OriginalStyle = State.Button->GetStyle();
        State.bEnabled = State.Button->GetIsEnabled();
        // The existing Blueprint's seventh button is Stage6_1, outside its six-entry row array.
        // Read that stage's progress too, while retaining its original Blueprint click handler.
        const FName ProgressRow = Rows.IsValidIndex(I) ? Rows[I] : I==6 ? FName(TEXT("Stage07")) : NAME_None;
        State.bCleared = GI && !ProgressRow.IsNone() && GI->GetStageProgressState(ProgressRow) == ESFStageProgressState::Cleared;
        if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(State.Button->Slot)) State.OriginalLayout = ButtonSlot->GetLayout();
        if (UWidget* Content = State.Button->GetContent())
        {
            State.ContentOpacity = Content->GetRenderOpacity();
            Content->SetRenderOpacity(0.f);
        }
        FButtonStyle Transparent = State.OriginalStyle;
        Transparent.Normal.DrawAs = Transparent.Hovered.DrawAs = Transparent.Pressed.DrawAs = Transparent.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
        State.Button->SetStyle(Transparent);
        State.Button->SetIsEnabled(false);
        State.Button->OnClicked.Clear();
    }
    if (Controls[0].Button) Controls[0].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click1);
    if (Controls[1].Button) Controls[1].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click2);
    if (Controls[2].Button) Controls[2].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click3);
    if (Controls[3].Button) Controls[3].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click4);
    if (Controls[4].Button) Controls[4].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click5);
    if (Controls[5].Button) Controls[5].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click6);
    if (Controls[6].Button) Controls[6].Button->OnClicked.AddDynamic(this,&USFLobbyStagePresentation::Click7);
    const TCHAR* RoadNames[] = { TEXT("Road"),TEXT("Road_1"),TEXT("Road_2"),TEXT("Road_3"),TEXT("Road_4"),TEXT("Road_5") };
    for (const TCHAR* Name : RoadNames)
        if (UWidget* Road = Lobby->GetWidgetFromName(Name))
        {
            Roads.Add(Road); RoadVisibility.Add(Road->GetVisibility());
            Road->SetVisibility(ESlateVisibility::Collapsed);
        }
    const TCHAR* HeaderNames[] = { TEXT("Image_329"),TEXT("Back_Btn"),TEXT("Story_Btn"),TEXT("Setting_Btn") };
    // Atlas sprites are individually framed; no raster resampling or baked hover variants.
    const FLinearColor HeaderRegions[] = {
        FLinearColor(10.f/1774,86.f/887,1009.f/1774,424.f/887),
        FLinearColor(1217.f/1774,492.f/887,1585.f/1774,798.f/887),
        FLinearColor(1040.f/1774,156.f/887,1758.f/1774,401.f/887),
        FLinearColor(180.f/1774,533.f/887,833.f/1774,780.f/887) };
    const FVector2D HeaderSizes[] = { {480,162},{112,96},{300,104},{250,96} };
    const FVector2D HeaderPositions[] = { {150,22},{24,54},{-294,50},{-24,54} };
    for (int32 HeaderIndex=0; HeaderIndex<4; ++HeaderIndex)
    {
        FSFLobbyHeaderControl& Header=HeaderControls.AddDefaulted_GetRef();
        Header.Widget=Lobby->GetWidgetFromName(HeaderNames[HeaderIndex]);
        if (!Header.Widget) continue;
        Header.Transform=Header.Widget->GetRenderTransform();
        Header.Pivot=Header.Widget->GetRenderTransformPivot();
        Header.Opacity=Header.Widget->GetRenderOpacity();
        if (UCanvasPanelSlot* HeaderSlot=Cast<UCanvasPanelSlot>(Header.Widget->Slot))
        {
            Header.OriginalLayout=HeaderSlot->GetLayout();
            if (HeaderMaterial)
            {
                HeaderSlot->SetAnchors(FAnchors(HeaderIndex>=2 ? 1.f : 0.f,0.f));
                HeaderSlot->SetAlignment(FVector2D(HeaderIndex>=2 ? 1.f : 0.f,0.f));
                HeaderSlot->SetPosition(HeaderPositions[HeaderIndex]);
                HeaderSlot->SetSize(HeaderSizes[HeaderIndex]);
            }
        }
        FSlateBrush NewBrush;
        if (HeaderMaterial)
        {
            Header.Material=UMaterialInstanceDynamic::Create(HeaderMaterial,this);
            Header.Material->SetVectorParameterValue(TEXT("Region"),HeaderRegions[HeaderIndex]);
            Header.Material->SetScalarParameterValue(TEXT("Title"),HeaderIndex==0 ? 1.f : 0.f);
            NewBrush.SetResourceObject(Header.Material);
            NewBrush.DrawAs=ESlateBrushDrawType::Image;
            NewBrush.ImageSize=HeaderSizes[HeaderIndex];
        }
        if (UImage* TitleImage=Cast<UImage>(Header.Widget))
        {
            Header.OriginalImageBrush=TitleImage->GetBrush();
            if (Header.Material) TitleImage->SetBrush(NewBrush);
        }
        Header.Widget->SetRenderTransformPivot(FVector2D(.5,.5));
        if (UButton* Button=Cast<UButton>(Header.Widget))
        {
            Header.OriginalStyle=Button->GetStyle();
            FButtonStyle Responsive=Header.OriginalStyle;
            if (Header.Material)
                Responsive.Normal=Responsive.Hovered=Responsive.Pressed=Responsive.Disabled=NewBrush;
            Button->SetStyle(Responsive);
        }
    }
}

void USFLobbyStagePresentation::RestoreControls()
{
    LaunchIndex = INDEX_NONE;
    for (FSFLobbyStageControl& State : Controls)
        if (State.Button)
        {
            State.Button->OnClicked = State.OriginalClick;
            State.Button->SetStyle(State.OriginalStyle);
            State.Button->SetIsEnabled(State.bEnabled);
            if (UWidget* Content = State.Button->GetContent()) Content->SetRenderOpacity(State.ContentOpacity);
            if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(State.Button->Slot)) ButtonSlot->SetLayout(State.OriginalLayout);
        }
    for (int32 I=0; I<Roads.Num(); ++I) if (Roads[I]) Roads[I]->SetVisibility(RoadVisibility[I]);
    for (const FSFLobbyHeaderControl& Header : HeaderControls)
        if (Header.Widget)
        {
            Header.Widget->SetRenderTransform(Header.Transform);
            Header.Widget->SetRenderTransformPivot(Header.Pivot);
            Header.Widget->SetRenderOpacity(Header.Opacity);
            if (UButton* Button=Cast<UButton>(Header.Widget)) Button->SetStyle(Header.OriginalStyle);
            if (UImage* TitleImage=Cast<UImage>(Header.Widget)) TitleImage->SetBrush(Header.OriginalImageBrush);
            if (UCanvasPanelSlot* HeaderSlot=Cast<UCanvasPanelSlot>(Header.Widget->Slot)) HeaderSlot->SetLayout(Header.OriginalLayout);
        }
    HeaderControls.Empty();
    Controls.Empty(); Roads.Empty(); RoadVisibility.Empty();
}

void USFLobbyStagePresentation::RequestLaunch(int32 Index)
{
    if (!Controls.IsValidIndex(Index) || !Controls[Index].bEnabled || LaunchIndex != INDEX_NONE || Reveal(Time,Index)<1.f) return;
    LaunchIndex = Index;
    LaunchElapsed = 0.f;
    bPreviewLaunch = false;
}

void USFLobbyStagePresentation::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    Time = Atmosphere ? Atmosphere->GetPresentationTime() : Time+DeltaTime;
    const FVector2D Size = Geometry.GetLocalSize();
    const float Scale = FMath::Min(Size.X/1920.f,Size.Y/1080.f);
    for (int32 I=0; I<HeaderControls.Num(); ++I)
    {
        auto& Header=HeaderControls[I];
        if (!Header.Widget) continue;
        UButton* Button=Cast<UButton>(Header.Widget);
        bool bHover=Button && (Button->IsHovered() || Button->HasKeyboardFocus());
#if WITH_DEV_AUTOMATION_TESTS
        bHover |= PreviewHeaderHover==I;
#endif
        Header.Hover=FMath::FInterpTo(Header.Hover,bHover ? 1.f : 0.f,DeltaTime,12.f);
        Header.Press=FMath::FInterpTo(Header.Press,Button && Button->IsPressed() ? 1.f : 0.f,DeltaTime,22.f);
        if (Header.Material)
        {
            Header.Material->SetScalarParameterValue(TEXT("Elapsed"),Time);
            Header.Material->SetScalarParameterValue(TEXT("Hover"),Header.Hover);
            Header.Material->SetScalarParameterValue(TEXT("Press"),Header.Press);
        }
        const float Age=FMath::Max(0.f,Time-.08f-I*.055f);
        const float Arrive=FMath::Clamp(Age/.45f,0.f,1.f);
        const float Swing=FMath::Sin(Age*17.f)*FMath::Exp(-Age*6.f);
        FWidgetTransform Transform=Header.Transform;
        Transform.Translation+=FVector2D(0,-24.f*FMath::Square(1.f-Arrive)-Header.Hover*3.f+Header.Press*2.f);
        Transform.Scale*=1.f+Header.Hover*.045f-Header.Press*.065f;
        Transform.Angle+=Swing*(I==0 ? 3.f : 1.6f)+(I==0 ? FMath::Sin(Time*.75f)*.16f : 0.f);
        Header.Widget->SetRenderTransform(Transform);
        Header.Widget->SetRenderOpacity(Header.Opacity*FMath::SmoothStep(0.f,1.f,Arrive));
    }
    for (int32 I=0; I<Controls.Num(); ++I)
    {
        auto& State = Controls[I];
        if (!State.Button) continue;
        State.Button->SetIsEnabled(State.bEnabled && Reveal(Time,I)>=1.f && LaunchIndex==INDEX_NONE);
        bool bHover = State.Button->IsHovered() || State.Button->HasKeyboardFocus();
#if WITH_DEV_AUTOMATION_TESTS
        bHover |= PreviewHover == I;
#endif
        State.Hover = FMath::FInterpTo(State.Hover,bHover && State.bEnabled ? 1.f : 0.f,DeltaTime,10.f);
        if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(State.Button->Slot))
        {
            ButtonSlot->SetAnchors(FAnchors(Centers[I].X,Centers[I].Y));
            ButtonSlot->SetAlignment(FVector2D(.5,.5));
            ButtonSlot->SetPosition(FVector2D::ZeroVector);
            ButtonSlot->SetSize(FVector2D(210,225)*Scale);
        }
    }
    if (LaunchIndex != INDEX_NONE)
    {
        LaunchElapsed += DeltaTime;
        if (LaunchElapsed >= LaunchDuration)
        {
            const int32 Index = LaunchIndex;
            LaunchIndex = INDEX_NONE;
            // A local delegate copy survives the original handler removing this menu.
            FOnButtonClickedEvent Click = Controls.IsValidIndex(Index) ? Controls[Index].OriginalClick : FOnButtonClickedEvent();
            if (!bPreviewLaunch) Click.Broadcast();
        }
    }
}

int32 USFLobbyStagePresentation::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    const int32 Base = Super::NativePaint(Args,Geometry,CullRect,Elements,Layer,Style,bEnabled);
    if (!IconAtlas || Time<RevealStart) return Base;
    const FVector2D Size = Geometry.GetLocalSize();
    const float S = FMath::Min(Size.X/1920.f,Size.Y/1080.f);
    const FLinearColor Teal(.08f,.8f,.78f), Gold(.88f,.57f,.18f), Ink(.12f,.19f,.17f), Danger(.86f,.16f,.25f);
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Lines = [&](const TArray<FVector2D>& Points,FLinearColor C,float Width,int32 Z)
    { FSlateDrawElement::MakeLines(Elements,Base+Z,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width*S); };
    auto Ring = [&](FVector2D P,FVector2D Radius,FLinearColor C,float Width,int32 Z,float Start=0.f,float Sweep=2.f*PI)
    {
        TArray<FVector2D> Points; Points.Reserve(49);
        for (int32 J=0; J<=48; ++J) { float A=Start+Sweep*J/48.f; Points.Add(P+FVector2D(FMath::Cos(A)*Radius.X,FMath::Sin(A)*Radius.Y)); }
        Lines(Points,C,Width,Z);
    };
    auto Box = [&](FVector2D P,FVector2D Extent,FLinearColor C,int32 Z,const FSlateBrush* Brush)
    { FSlateDrawElement::MakeBox(Elements,Base+Z,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,C); };
    auto Text = [&](const FString& Value,FVector2D P,int32 FontSize,FLinearColor C,int32 Z)
    {
        FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle("Bold",FMath::Max(8,FMath::RoundToInt(FontSize*S)));
        FSlateDrawElement::MakeText(Elements,Base+Z,Geometry.ToPaintGeometry(FVector2D(200,40)*S,FSlateLayoutTransform(P)),Value,Font,ESlateDrawEffect::None,C);
    };
    // Dotted routes stop short of the icon bases. They appear with the pins, never over an opening scroll.
    for (int32 I=0; I<6; ++I)
    {
        const float Alpha=Reveal(Time,I+1)*.64f;
        FVector2D A=Centers[I]*Size+FVector2D(0,48)*S, B=Centers[I+1]*Size+FVector2D(0,48)*S;
        for (int32 J=1; J<17; ++J)
        {
            float U=J/17.f; FVector2D P=FMath::Lerp(A,B,U);
            if ((P-A).Size()<90*S || (P-B).Size()<90*S) continue;
            P.Y+=FMath::Sin(U*PI)*18*S;
            Ring(P,FVector2D(2.4,2.4)*S,FLinearColor(.39,.25,.10,Alpha),3,1);
        }
    }
    for (int32 I=0; I<Controls.Num(); ++I)
    {
        const auto& State=Controls[I];
        float Alpha=Reveal(Time,I);
        if (Alpha<=0.f) continue;
        const bool Boss=I==6;
        FLinearColor Accent=State.bCleared ? Gold : Boss ? Danger : Teal;
        const float H=State.Hover;
        const bool Launch=LaunchIndex==I;
        if (LaunchIndex!=INDEX_NONE && !Launch) Alpha*=.65f;
        FVector2D P=Centers[I]*Size+FVector2D(0,(1-Alpha)*18-H*7)*S;
        FVector2D Foot=P+FVector2D(0,55)*S;
        FLinearColor Rim=State.bEnabled ? Accent : FLinearColor(.40,.36,.26);
        Rim.A=Alpha*(State.bEnabled ? .65f+.2f*FMath::Sin(Time*2) : .65f);
        Ring(Foot,FVector2D(81,24)*S,FLinearColor(.12,.13,.10,Alpha*.25),7,2);
        Ring(Foot,FVector2D(83,25)*S,Rim,2,3);
        if (H>.01f || Launch)
        {
            FLinearColor Glow=Accent; Glow.A=Alpha*(Launch ? 1.f : H)*.8f;
            for (int32 Arc=0; Arc<3; ++Arc)
                Ring(Foot,FVector2D(91,31)*S,Glow,2,4,Time*1.5f+Arc*2.094f,1.1f);
            for (int32 Spark=0; Spark<5; ++Spark)
            {
                float A=Time*.9f+Spark*1.257f;
                FVector2D Q=P+FVector2D(FMath::Cos(A)*98,FMath::Sin(A)*37-5)*S;
                Lines({Q-FVector2D(4,0)*S,Q+FVector2D(4,0)*S},Glow,1.4,7);
                Lines({Q-FVector2D(0,4)*S,Q+FVector2D(0,4)*S},Glow,1.4,7);
            }
        }
        // The same tower identity survives as rubble after a clear. Progress is still owned by GameInstance.
        const bool bDrawRuin=State.bCleared && ClearedIcon;
        FSlateBrush Brush; Brush.SetResourceObject(bDrawRuin ? ClearedIcon.Get() : IconAtlas.Get()); Brush.DrawAs=ESlateBrushDrawType::Image;
        if (!bDrawRuin) Brush.SetUVRegion(FBox2D(FVector2D(.5,0),FVector2D(.75,.5)));
        const float IconSize=(Boss ? 211.f : 200.f)*(1+.035f*H)*S;
        FLinearColor Tint=Boss ? (State.bCleared ? FLinearColor(.85,.71,.94,Alpha) : FLinearColor(.61,.39,.78,Alpha)) : FLinearColor(1,1,1,Alpha);
        if (!State.bEnabled && !State.bCleared) Tint=FLinearColor(.29,.30,.28,Alpha*.95f);
        Box(P-FVector2D(IconSize*.5,IconSize*.57)+FVector2D(0,bDrawRuin ? 10*S : 0),FVector2D(IconSize),Tint,5,&Brush);
        if (State.bCleared)
        {
            // A small gold victory seal also distinguishes a conquered ruin from a locked stage.
            const FVector2D Seal=Foot+FVector2D(69,-5)*S;
            Ring(Seal,FVector2D(13,13)*S,FLinearColor(.19,.24,.18,Alpha),13,6);
            Ring(Seal,FVector2D(14,14)*S,FLinearColor(Gold.R,Gold.G,Gold.B,Alpha),2,7);
            Lines({Seal+FVector2D(-6,0)*S,Seal+FVector2D(-1,5)*S,Seal+FVector2D(7,-5)*S},FLinearColor(1.f,.86f,.45f,Alpha),3,8);
        }
        const FVector2D Label=P+FVector2D(-75,77)*S;
        Box(Label,FVector2D(150,32)*S,FLinearColor(Ink.R,Ink.G,Ink.B,Alpha*.94),6,White);
        Box(Label,FVector2D(150,2)*S,FLinearColor(Rim.R,Rim.G,Rim.B,Alpha),7,White);
        Text(FString::Printf(TEXT("STAGE %02d"),I+1),Label+FVector2D(20,3)*S,20,FLinearColor(.96,.88,.66,Alpha),8);
        FString Status=Launch ? TEXT("DEPLOYING") : State.bCleared ? TEXT("CLEARED") : !State.bEnabled ? TEXT("LOCKED") : Boss ? TEXT("FINAL BATTLE") : TEXT("READY");
        float TextOffset=Status.Len()*4.f;
        Text(Status,P+FVector2D(-TextOffset,114)*S,13,FLinearColor(Boss ? .48f : .22f,.20f,.17f,Alpha),8);
        if (!State.bEnabled && !State.bCleared)
        {
            const FLinearColor Shadow(.045f,.045f,.035f,Alpha);
            const FLinearColor Iron(.48f,.43f,.32f,Alpha);
            const FLinearColor Edge(.86f,.72f,.43f,Alpha);
            // Crossed linked chains seal the tower silhouette even at small UI scales.
            for (int32 Direction : {-1,1})
            {
                const FVector2D A=P+FVector2D(-66,-62*Direction)*S;
                const FVector2D B=P+FVector2D(66,48*Direction)*S;
                Lines({A,B},Shadow,10,7);
                for (int32 Link=0;Link<9;++Link)
                {
                    const FVector2D Q=FMath::Lerp(A,B,(Link+.5f)/9.f);
                    const FVector2D Along=FVector2D(8,6.7f*Direction)*S;
                    const FVector2D Across=FVector2D(-3.8f*Direction,4.5f)*S;
                    Lines({Q-Along,Q-Across,Q+Along,Q+Across,Q-Along},Iron,3.5f,8);
                }
            }
            const FVector2D Lock=P+FVector2D(0,2)*S;
            Ring(Lock-FVector2D(0,19)*S,FVector2D(16,20)*S,Shadow,10,8,PI,PI);
            Ring(Lock-FVector2D(0,19)*S,FVector2D(16,20)*S,Edge,5,8,PI,PI);
            Box(Lock-FVector2D(29,19)*S,FVector2D(58,47)*S,Shadow,8,White);
            Box(Lock-FVector2D(26,16)*S,FVector2D(52,41)*S,Edge,8,White);
            Box(Lock-FVector2D(22,12)*S,FVector2D(44,33)*S,FLinearColor(.24f,.20f,.13f,Alpha),8,White);
            Ring(Lock-FVector2D(0,2)*S,FVector2D(4,4)*S,Edge,4,9);
            Box(Lock+FVector2D(-2,0)*S,FVector2D(4,11)*S,Edge,9,White);
        }
        if (Boss && !State.bCleared)
        {
            for (int32 J=0; J<3; ++J)
            {
                FVector2D Q=P+FVector2D((J-1)*16,-92)*S;
                Lines({Q+FVector2D(-5,0)*S,Q+FVector2D(0,-7)*S,Q+FVector2D(5,0)*S,Q+FVector2D(0,7)*S,Q+FVector2D(-5,0)*S},FLinearColor(.69,.13,.22,Alpha),2,7);
            }
        }
        if (Launch)
        {
            float U=FMath::Clamp(LaunchElapsed/LaunchDuration,0.f,1.f);
            for (int32 J=0; J<2; ++J)
            {
                float Wave=FMath::Frac(U*1.5f+J*.5f);
                Ring(Foot,FVector2D(84+Wave*80,26+Wave*35)*S,FLinearColor(Accent.R,Accent.G,Accent.B,(1-Wave)*Alpha),3,9);
            }
            Ring(P,FVector2D(108,108)*S,FLinearColor(Accent.R,Accent.G,Accent.B,Alpha),3,9,-PI*.5,U*2*PI);
        }
    }
    return Base+10;
}
