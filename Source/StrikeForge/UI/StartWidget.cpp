#include "StartWidget.h"
#include "Game/Audio/SFAudioLibrary.h"
#include "SFStartButtonFeedbackWidget.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"
#include "SFMenuAtmosphereWidget.h"
#include "TimerManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/WidgetTree.h"
#include "SFStartSceneAmbience.h"
#include "Components/TextBlock.h"
#include "Components/ButtonSlot.h"
#include "Engine/Font.h"
#include "Styling/SlateBrush.h"

UStartWidget::UStartWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Logo(TEXT("/Game/Core/UI/LobbyAtmosphere/T_StartEtherOath.T_StartEtherOath"));
    Wordmark = Logo.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LogoEffect(TEXT("/Game/Core/UI/LobbyAtmosphere/M_StartEtherOathLogo.M_StartEtherOathLogo"));
    LogoMaterial = LogoEffect.Object;
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    MenuFont = Font.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shade(TEXT("/Game/Core/UI/LobbyAtmosphere/M_StartMenuShade.M_StartMenuShade"));
    MenuShadeMaterial = Shade.Object;
}

void UStartWidget::ApplyStartArt()
{
    if (!WidgetTree || !ArtControls.IsEmpty()) return;
    USFAudioLibrary::ApplyMenuButtonSounds(this);
    ArtElapsed = 0.f;
    // Remove the old global blur / black veil; light belongs to the 3D scene.
    if (UWidget* Backdrop = GetWidgetFromName(TEXT("BackgroundBlur_149")))
    {
        OriginalBackdropVisibility = Backdrop->GetVisibility();
        Backdrop->SetVisibility(ESlateVisibility::Collapsed);
    }
    UWidget* Widgets[] = { Title, Start_Btn, Setting_Btn, Exit_Btn };
    const FText Labels[] = {
        NSLOCTEXT("SFStart", "Start", "게임 시작"),
        NSLOCTEXT("SFStart", "Settings", "설정"),
        NSLOCTEXT("SFStart", "Exit", "게임 종료") };
    for (int32 I=0; I<4; ++I)
    {
        FSFStartArtControl& State = ArtControls.AddDefaulted_GetRef();
        State.Widget = Widgets[I];
        if (!State.Widget) continue;
        State.Transform = State.Widget->GetRenderTransform();
        if (auto* ArtLayoutSlot = Cast<UCanvasPanelSlot>(State.Widget->Slot))
        {
            State.OriginalLayout = ArtLayoutSlot->GetLayout();
            ArtLayoutSlot->SetAnchors(FAnchors(.5f, I==0 ? .28f : .65f));
            ArtLayoutSlot->SetAlignment(FVector2D(.5f,.5f));
            ArtLayoutSlot->SetSize(I==0 ? FVector2D(1050,350) : FVector2D(300,54));
            ArtLayoutSlot->SetPosition(FVector2D(0,I==0 ? 0 : (I-1)*64));
        }
        if (auto* Image = Cast<UImage>(State.Widget))
        {
            State.OriginalImage = Image->GetBrush();
            if (LogoMaterial)
            {
                Image->SetBrushFromMaterial(LogoMaterial);
                State.Material = Image->GetDynamicMaterial();
                if (State.Material && Wordmark) State.Material->SetTextureParameterValue(TEXT("Logo"),Wordmark);
            }
            else if (Wordmark) Image->SetBrushFromTexture(Wordmark);
        }
        if (auto* Button = Cast<UButton>(State.Widget))
        {
            State.OriginalStyle = Button->GetStyle();
            State.OriginalContent = Button->GetContent();
            FButtonStyle Style = State.OriginalStyle;
            FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
            Style.Normal = Style.Hovered = Style.Pressed = Style.Disabled = Empty;
            Style.NormalPadding = Style.PressedPadding = FMargin(0);
            Button->SetStyle(Style);
            State.Feedback = CreateWidget<USFStartButtonFeedbackWidget>(GetOwningPlayer());
            if (!State.Feedback) continue;
            State.Label = State.Feedback->CreateLabel();
            State.Label->SetText(Labels[I-1]);
            State.Label->SetFont(FSlateFontInfo(MenuFont,I==1 ? 28 : 22));
            State.Label->SetJustification(ETextJustify::Center);
            State.Label->SetColorAndOpacity(FSlateColor(FLinearColor(.94f,.91f,.82f,1.f)));
            State.Label->SetShadowColorAndOpacity(FLinearColor(0.018f,.023f,.023f,.9f));
            State.Label->SetShadowOffset(FVector2D(0,2));
            State.Label->SetVisibility(ESlateVisibility::HitTestInvisible);
            Button->SetContent(State.Feedback);
            if (auto* ButtonSlot=Cast<UButtonSlot>(State.Feedback->Slot))
            {
                ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
                ButtonSlot->SetVerticalAlignment(VAlign_Fill);
            }
        }
    }
}

void UStartWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    ArtElapsed += DeltaTime;
    for (int32 I=0; I<ArtControls.Num(); ++I)
    {
        auto& State=ArtControls[I];
        if (!State.Widget) continue;
        auto* Button=Cast<UButton>(State.Widget);
        const bool Hover=Button && (Button->IsHovered() || Button->HasKeyboardFocus()) && !bEnteringLobby
            && (!SettingWidget || !SettingWidget->IsVisible());
        if (Hover && !State.bWasHovered) State.HoverTime=0.f;
        State.bWasHovered=Hover;
        State.HoverTime+=DeltaTime;
        State.ClickTime=FMath::Min(1.f,State.ClickTime+DeltaTime);
        State.Hover=FMath::FInterpTo(State.Hover,Hover ? 1.f : 0.f,DeltaTime,14.f);
        State.Press=FMath::FInterpTo(State.Press,Button && Button->IsPressed() ? 1.f : 0.f,DeltaTime,24.f);
        if (State.Label)
        {
            State.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(FLinearColor(.94f,.91f,.82f),FLinearColor(.24f,1.f,.83f),State.Hover)));
            const float Kick=FMath::Max(0.f,1.f-State.ClickTime/.32f);
            State.Label->SetRenderTransformPivot(FVector2D(.5f,.5f));
            State.Label->SetRenderScale(FVector2D(1.f+.035f*State.Hover-.045f*State.Press+.025f*Kick));
            State.Label->SetRenderTranslation(FVector2D(0,State.Press*2));
            State.Label->SetShadowColorAndOpacity(FMath::Lerp(FLinearColor(.018f,.023f,.023f,.9f),
                FLinearColor(.015f,.24f,.19f,.85f),State.Hover));
        }
        if (State.Feedback)
            State.Feedback->SetFeedback(State.Hover,State.HoverTime,State.ClickTime,
                !bEnteringLobby && (!SettingWidget || !SettingWidget->IsVisible()));
        if (I==0 && State.Material)
            State.Material->SetScalarParameterValue(TEXT("Elapsed"),ArtElapsed);
        // Anchors follow aspect changes; a wide title also fits narrow / small windows.
        if (I==0)
            if (auto* ArtLayoutSlot=Cast<UCanvasPanelSlot>(State.Widget->Slot))
            {
                const float Width=FMath::Min(1050.f,Geometry.GetLocalSize().X*.68f);
                ArtLayoutSlot->SetSize(FVector2D(Width,Width/3.f));
            }
    }
    if (IsValid(SceneAmbience) && Title)
        SceneAmbience->UpdateLogoParticles(Title->GetCachedGeometry());
}

void UStartWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyStartArt();
	if (!MenuShade && MenuShadeMaterial && WidgetTree)
	{
		if (UCanvasPanel* Canvas=Cast<UCanvasPanel>(GetRootWidget()))
		{
			MenuShade=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("RightMenuShade"));
			MenuShade->SetBrushFromMaterial(MenuShadeMaterial);
			MenuShade->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* ShadeSlot=Canvas->AddChildToCanvas(MenuShade);
			ShadeSlot->SetAnchors(FAnchors(0,0,1,1));
			ShadeSlot->SetOffsets(FMargin(0));
			ShadeSlot->SetZOrder(-1);
		}
	}
	if (!SceneAmbience && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags|=RF_Transient;
		Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SceneAmbience=GetWorld()->SpawnActor<ASFStartSceneAmbience>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
		if (SceneAmbience) SceneAmbience->SetViewer(GetOwningPlayer());
	}

	// 버튼 클릭 이벤트 바인딩
	if (IsValid(Start_Btn))
	{
		Start_Btn->OnPressed.AddUniqueDynamic(this, &UStartWidget::OnStartButtonPressed);
		Start_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnStartButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnPressed.AddUniqueDynamic(this, &UStartWidget::OnSettingButtonPressed);
		Setting_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnSettingButtonClicked);
	}
	if (IsValid(Exit_Btn))
	{
		Exit_Btn->OnPressed.AddUniqueDynamic(this, &UStartWidget::OnExitButtonPressed);
		Exit_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnExitButtonClicked);
	}
}

void UStartWidget::PlayButtonFeedback(int32 Index)
{
    if (bEnteringLobby || (SettingWidget && SettingWidget->IsVisible())) return;
    if (ArtControls.IsValidIndex(Index)) ArtControls[Index].ClickTime=0.f;
}

void UStartWidget::OnStartButtonPressed() { PlayButtonFeedback(1); }
void UStartWidget::OnSettingButtonPressed() { PlayButtonFeedback(2); }
void UStartWidget::OnExitButtonPressed() { PlayButtonFeedback(3); }

void UStartWidget::OnStartButtonClicked()
{
	if (bEnteringLobby) return;
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	bEnteringLobby = true;
	DepartureMist = CreateWidget<USFMenuAtmosphereWidget>(GetOwningPlayer());
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
	if (DepartureMist && DepartureMist->IsReady() && Canvas)
	{
		DepartureMist->SetDeparture(true);
		int32 TopZOrder = 0;
		for (UWidget* Child : Canvas->GetAllChildren())
		{
			if (const UCanvasPanelSlot* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot))
				TopZOrder = FMath::Max(TopZOrder, ChildSlot->GetZOrder());
		}
		// Stay in the same Slate layer as the menu, including AddToPlayerScreen hosts.
		UCanvasPanelSlot* MistSlot = Canvas->AddChildToCanvas(DepartureMist);
		MistSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		MistSlot->SetOffsets(FMargin(0.f));
		MistSlot->SetZOrder(TopZOrder + 1);
		GetWorld()->GetTimerManager().SetTimer(LobbyTransitionTimer, this,
			&UStartWidget::FinishLobbyTransition, USFMenuAtmosphereWidget::DepartureDuration, false);
	}
	else FinishLobbyTransition();
}

void UStartWidget::FinishLobbyTransition()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!GameInstance || !GameInstance->MoveToLobby())
	{
		if (DepartureMist) DepartureMist->RemoveFromParent();
		DepartureMist = nullptr;
		bEnteringLobby = false;
	}
}

void UStartWidget::NativeDestruct()
{
	if (IsValid(SceneAmbience)) SceneAmbience->Destroy();
	SceneAmbience=nullptr;
	if (MenuShade) MenuShade->RemoveFromParent();
	MenuShade=nullptr;
	for (const FSFStartArtControl& State : ArtControls)
	{
		if (!State.Widget) continue;
		State.Widget->SetRenderTransform(State.Transform);
		if (UCanvasPanelSlot* ArtSlot=Cast<UCanvasPanelSlot>(State.Widget->Slot)) ArtSlot->SetLayout(State.OriginalLayout);
		if (UImage* Image=Cast<UImage>(State.Widget)) Image->SetBrush(State.OriginalImage);
		if (UButton* Button=Cast<UButton>(State.Widget))
        {
            Button->SetStyle(State.OriginalStyle);
            Button->SetContent(State.OriginalContent);
        }
	}
	ArtControls.Empty();
    if (UWidget* Backdrop=GetWidgetFromName(TEXT("BackgroundBlur_149"))) Backdrop->SetVisibility(OriginalBackdropVisibility);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LobbyTransitionTimer);
	if (DepartureMist) DepartureMist->RemoveFromParent();
	DepartureMist = nullptr;
	bEnteringLobby = false;
	Super::NativeDestruct();
}

void UStartWidget::OnSettingButtonClicked()
{
	if (IsValid(SettingWidget))
	{
		SettingWidget->OpenSetting();
	}
}

void UStartWidget::OnExitButtonClicked()
{
	APlayerController* SpecificPlayer = GetOwningPlayer();
	if (IsValid(SpecificPlayer))
	{
		UKismetSystemLibrary::QuitGame(this, SpecificPlayer, EQuitPreference::Quit, false);
	}
}
