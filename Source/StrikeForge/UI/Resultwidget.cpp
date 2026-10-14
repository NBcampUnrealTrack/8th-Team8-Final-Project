#include "ResultWidget.h"
#include "SFResultPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Brushes/SlateNoResource.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"

UResultWidget::UResultWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<USoundWave> Sound(TEXT("/Game/Sound/UI/S_EtherVictory.S_EtherVictory"));
    VictorySound=Sound.Object;
    static ConstructorHelpers::FObjectFinder<USoundWave> Defeat(TEXT("/Game/Sound/UI/S_EtherDefeat.S_EtherDefeat"));
    DefeatSound=Defeat.Object;
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    VictoryFont=Font.Object;
    static ConstructorHelpers::FObjectFinder<UFontFace> TitleFace(TEXT("/Game/Game/IN_UI/Font/NanumMyeongjo_ExtraBold.NanumMyeongjo_ExtraBold"));
    ResultTitleFace=TitleFace.Object;
}

void UResultWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (ResultTitleFace && !ResultTitleFont)
    {
        ResultTitleFont=NewObject<UFont>(this);
        ResultTitleFont->FontCacheType=EFontCacheType::Runtime;
        FTypefaceEntry Entry(TEXT("Default"));
        Entry.Font=FFontData(ResultTitleFace);
        ResultTitleFont->CompositeFont.DefaultTypeface.Fonts.Add(Entry);
    }
    if (Exit_Btn_W) Exit_Btn_W->OnClicked.AddUniqueDynamic(this,&UResultWidget::OnExitButtonClicked);
    if (Exit_Btn_F) Exit_Btn_F->OnClicked.AddUniqueDynamic(this,&UResultWidget::OnExitButtonClicked);
    for (UWidget* Widget : {Win_Image,Loose_Image,static_cast<UWidget*>(Exit_Btn_W),static_cast<UWidget*>(Exit_Btn_F),static_cast<UWidget*>(Win_Glow.Get()),static_cast<UWidget*>(Flash.Get())})
        if (Widget) Widget->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* OldShade=GetWidgetFromName(TEXT("Border_46"))) OldShade->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        if (!Presentation)
        {
            Presentation=WidgetTree->ConstructWidget<USFResultPresentation>(USFResultPresentation::StaticClass(),TEXT("ResultAtmosphere"));
            Presentation->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* BackSlot=Canvas->AddChildToCanvas(Presentation);
            BackSlot->SetAnchors(FAnchors(0,0,1,1)); BackSlot->SetOffsets(FMargin(0)); BackSlot->SetZOrder(-5);
            ForegroundSparkles=WidgetTree->ConstructWidget<USFResultPresentation>(USFResultPresentation::StaticClass(),TEXT("ResultGemSparkles"));
            ForegroundSparkles->bForeground=true;
            ForegroundSparkles->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* SparkleSlot=Canvas->AddChildToCanvas(ForegroundSparkles);
            SparkleSlot->SetAnchors(FAnchors(0,0,1,1)); SparkleSlot->SetOffsets(FMargin(0)); SparkleSlot->SetZOrder(5);
        }
        if (!EtherPreviewExit)
        {
            EtherPreviewExit=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("EtherPreviewExit"));
            EtherPreviewExit->OnClicked.AddUniqueDynamic(this,&UResultWidget::OnExitButtonClicked);
            EtherPreviewExit->SetVisibility(ESlateVisibility::Collapsed);
            FButtonStyle Style;
            Style.SetNormal(FSlateNoResource());
            Style.SetHovered(FSlateNoResource());
            Style.SetPressed(FSlateNoResource());
            Style.SetDisabled(FSlateNoResource());
            Style.SetNormalPadding(FMargin(0));
            Style.SetPressedPadding(FMargin(0));
            EtherPreviewExit->SetStyle(Style);
            auto* Content=WidgetTree->ConstructWidget<UOverlay>();
            auto* ContentSlot=Cast<UButtonSlot>(EtherPreviewExit->AddChild(Content));
            ContentSlot->SetHorizontalAlignment(HAlign_Fill);
            ContentSlot->SetVerticalAlignment(VAlign_Fill);
            ContentSlot->SetPadding(FMargin(0));
            ExitDecoration=WidgetTree->ConstructWidget<USFResultPresentation>();
            ExitDecoration->bExitButtonDecoration=true;
            ExitDecoration->ExitButton=EtherPreviewExit;
            ExitDecoration->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* DecorationSlot=Content->AddChildToOverlay(ExitDecoration);
            DecorationSlot->SetHorizontalAlignment(HAlign_Fill);
            DecorationSlot->SetVerticalAlignment(VAlign_Fill);
            auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("EtherPreviewExitLabel"));
            Label->SetText(FText::FromString(TEXT("로비로 돌아가기")));
            Label->SetColorAndOpacity(FSlateColor(FLinearColor(.77f,.91f,.98f)));
            auto* Font=VictoryFont.Get();
            Label->SetFont(Font?FSlateFontInfo(Font,24):FCoreStyle::GetDefaultFontStyle("Bold",24));
            if(Presentation)Presentation->PreviewFont=ResultTitleFont?ResultTitleFont.Get():Font;
            auto* LabelSlot=Content->AddChildToOverlay(Label);
            LabelSlot->SetHorizontalAlignment(HAlign_Center);
            LabelSlot->SetVerticalAlignment(VAlign_Center);
            LabelSlot->SetPadding(FMargin(56,8));
            Label->SetShadowOffset(FVector2D(0,2));
            Label->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85f));
            auto* ExitSlot=Canvas->AddChildToCanvas(EtherPreviewExit);
            ExitSlot->SetZOrder(10);
        }
    }
}

void UResultWidget::NativeDestruct()
{
    if(IsValid(EtherPreviewAudio))EtherPreviewAudio->Stop();
    EtherPreviewAudio=nullptr;bPresenting=false;
    Super::NativeDestruct();
}

void UResultWidget::SetResultState(bool bIsWin)
{
    StopAllAnimations();
    bVictory=bIsWin; bPresenting=true;
    bEtherBurstPreview=true;bPreviewExitFocused=false;
    if(IsValid(EtherPreviewAudio))EtherPreviewAudio->Stop();
    EtherPreviewAudio=nullptr;
    ResultStartTime=FSlateApplication::Get().GetCurrentTime();
    if (Presentation) { Presentation->bVictory=bVictory; Presentation->StartTime=ResultStartTime; Presentation->bEtherBurstPreview=bEtherBurstPreview; }
    if (ForegroundSparkles) { ForegroundSparkles->bVictory=bVictory; ForegroundSparkles->StartTime=ResultStartTime; ForegroundSparkles->bEtherBurstPreview=bEtherBurstPreview; }
    if (ExitDecoration) { ExitDecoration->bVictory=bVictory; ExitDecoration->StartTime=ResultStartTime; }
    if (Win_Image) Win_Image->SetVisibility(bVictory && !bEtherBurstPreview?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if (Loose_Image) Loose_Image->SetVisibility(bVictory || bEtherBurstPreview?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
    if (Exit_Btn_W) Exit_Btn_W->SetVisibility(bVictory && !bEtherBurstPreview?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if (Exit_Btn_F) Exit_Btn_F->SetVisibility(bVictory || bEtherBurstPreview?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    if (Win_Glow) Win_Glow->SetVisibility(ESlateVisibility::Collapsed);
    if (Flash) Flash->SetVisibility(ESlateVisibility::Collapsed);
    if(EtherPreviewExit)EtherPreviewExit->SetVisibility(bEtherBurstPreview?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    USoundWave* ResultSound=bVictory?VictorySound.Get():DefeatSound.Get();
    if(bEtherBurstPreview && ResultSound)
    {
        EtherPreviewAudio=UGameplayStatics::CreateSound2D(this,ResultSound,.8f,1.f,0.f,nullptr,false,true);
        if(EtherPreviewAudio){EtherPreviewAudio->SetUISound(true);EtherPreviewAudio->Play();}
    }
    // Pause the simulation; Slate's real-time clock drives the presentation.
    UGameplayStatics::SetGamePaused(this,true);
    if (auto* PC=GetOwningPlayer())
    {
        FInputModeUIOnly Input;
        if (UButton* Exit=bEtherBurstPreview?nullptr:(bVictory?Exit_Btn_W:Exit_Btn_F)) Input.SetWidgetToFocus(Exit->TakeWidget());
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Input); PC->bShowMouseCursor=true;
    }
    NativeTick(GetCachedGeometry(),0.f);
}

void UResultWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);
    if (!bPresenting) return;
    const FVector2D Size=Geometry.GetLocalSize(); if (Size.X<1 || Size.Y<1) return;
    const float Fit=FMath::Min(Size.X/1920.,Size.Y/1080.);
    const float Age=FMath::Max(0.,FSlateApplication::Get().GetCurrentTime()-ResultStartTime);
    const float P=FMath::Clamp(Age/.8f,0.f,1.f);
    const float Ease=1.f-FMath::Pow(1.f-P,3.f);
    auto Layout=[&](UWidget* Widget,FVector2D Anchor,FVector2D Extent)
    {
        if (!Widget) return;
        if (auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            Slot->SetAnchors(FAnchors(Anchor.X,Anchor.Y)); Slot->SetAlignment(FVector2D(.5,.5));
            Slot->SetPosition(FVector2D::ZeroVector); Slot->SetSize(Extent*Fit);
        }
        Widget->SetRenderTransformPivot(FVector2D(.5,.5));
    };
    UWidget* Crest=bEtherBurstPreview?nullptr:(bVictory?Win_Image:Loose_Image);
    Layout(Crest,FVector2D(.5,.42),FVector2D(1000,620));
    if (Crest)
    {
        const float Scale=.76f+.24f*Ease+.025f*FMath::Sin(P*PI);
        Crest->SetRenderScale(FVector2D(Scale)); Crest->SetRenderTranslation(FVector2D(0,(1-Ease)*45*Fit));
        Crest->SetRenderOpacity(FMath::Clamp(Age/.3f,0.f,1.f));
    }
    UButton* Exit=bEtherBurstPreview?EtherPreviewExit.Get():(bVictory?Exit_Btn_W:Exit_Btn_F);
    Layout(Exit,FVector2D(.5,bEtherBurstPreview?.82:.86),bEtherBurstPreview?FVector2D(420,88):FVector2D(353,149));
    if (Exit)
    {
        const float Hover=Exit->IsHovered()?1.04f:1.f;
        Exit->SetRenderScale(FVector2D(Exit->IsPressed()?.98f:Hover));
        Exit->SetRenderOpacity(FMath::Clamp((Age-(bEtherBurstPreview?2.9f:.7f))/.4f,0.f,1.f));
        if(bEtherBurstPreview)
        {
            Exit->SetIsEnabled(Age>=3.25f);
            if(Age>=3.25f && !bPreviewExitFocused){Exit->SetKeyboardFocus();bPreviewExitFocused=true;}
        }
    }
    // Non-looping result stingers finish naturally; leaving the screen stops them in NativeDestruct.
}

void UResultWidget::OnExitButtonClicked()
{
    USFGameInstance* Instance=GetGameInstance<USFGameInstance>();
    if (!Instance) return;
    UGameplayStatics::SetGamePaused(this,false);
    Instance->MoveToLobby();
}
