#include "StartWidget.h"
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

UStartWidget::UStartWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Core/UI/LobbyAtmosphere/M_LobbyHeader.M_LobbyHeader"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Atlas(TEXT("/Game/Core/UI/LobbyAtmosphere/T_StartMenuAtlas.T_StartMenuAtlas"));
	MenuArtMaterial=Material.Object;
	MenuArtAtlas=Atlas.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shade(TEXT("/Game/Core/UI/LobbyAtmosphere/M_StartMenuShade.M_StartMenuShade"));
	MenuShadeMaterial=Shade.Object;
}

void UStartWidget::ApplyStartArt()
{
	if (!MenuArtMaterial || !MenuArtAtlas || !ArtControls.IsEmpty()) return;
	ArtElapsed=0.f;
	UWidget* Widgets[] = { Title,Start_Btn,Setting_Btn,Exit_Btn };
	const FLinearColor Regions[] = {
		FLinearColor(28.f/1774,25.f/887,929.f/1774,449.f/887),
		FLinearColor(975.f/1774,154.f/887,1736.f/1774,441.f/887),
		FLinearColor(72.f/1774,532.f/887,848.f/1774,819.f/887),
		FLinearColor(974.f/1774,532.f/887,1737.f/1774,819.f/887) };
	for (int32 I=0; I<4; ++I)
	{
		FSFStartArtControl& State=ArtControls.AddDefaulted_GetRef();
		State.Widget=Widgets[I];
		if (!State.Widget) continue;
		State.Transform=State.Widget->GetRenderTransform();
		State.Material=UMaterialInstanceDynamic::Create(MenuArtMaterial,this);
		State.Material->SetTextureParameterValue(TEXT("Atlas"),MenuArtAtlas);
		State.Material->SetVectorParameterValue(TEXT("Region"),Regions[I]);
		State.Material->SetScalarParameterValue(TEXT("Title"),I==0 ? 1.f : 0.f);
		FSlateBrush Brush;
		Brush.DrawAs=ESlateBrushDrawType::Image;
		Brush.SetResourceObject(State.Material);
		Brush.ImageSize=I==0 ? FVector2D(700,330) : I==1 ? FVector2D(360,132) : FVector2D(300,110);
		if (UCanvasPanelSlot* ArtSlot=Cast<UCanvasPanelSlot>(State.Widget->Slot))
		{
			State.OriginalLayout=ArtSlot->GetLayout();
			ArtSlot->SetSize(Brush.ImageSize);
			ArtSlot->SetAnchors(I==0 ? FAnchors(.5f,0.f) : FAnchors(1.f,1.f));
			ArtSlot->SetAlignment(I==0 ? FVector2D(.5,0) : FVector2D(1,1));
			const FVector2D Positions[] = { {0,24},{-80,-298},{-80,-172},{-80,-46} };
			ArtSlot->SetPosition(Positions[I]);
		}
		if (UImage* Image=Cast<UImage>(State.Widget))
		{
			State.OriginalImage=Image->GetBrush();
			Image->SetBrush(Brush);
		}
		if (UButton* Button=Cast<UButton>(State.Widget))
		{
			State.OriginalStyle=Button->GetStyle();
			FButtonStyle Style=State.OriginalStyle;
			Style.Normal=Style.Hovered=Style.Pressed=Style.Disabled=Brush;
			Button->SetStyle(Style);
		}
	}
}

void UStartWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	ArtElapsed+=DeltaTime;
	for (FSFStartArtControl& State : ArtControls)
	{
		if (!State.Widget || !State.Material) continue;
		UButton* Button=Cast<UButton>(State.Widget);
		const bool Hover=Button && (Button->IsHovered() || Button->HasKeyboardFocus()) && !bEnteringLobby;
		State.Hover=FMath::FInterpTo(State.Hover,Hover ? 1.f : 0.f,DeltaTime,12.f);
		State.Press=FMath::FInterpTo(State.Press,Button && Button->IsPressed() ? 1.f : 0.f,DeltaTime,22.f);
		State.Material->SetScalarParameterValue(TEXT("Elapsed"),ArtElapsed);
		State.Material->SetScalarParameterValue(TEXT("Hover"),State.Hover);
		State.Material->SetScalarParameterValue(TEXT("Press"),State.Press);
		FWidgetTransform Transform=State.Transform;
		Transform.Scale*=1.f+State.Hover*.035f-State.Press*.05f;
		Transform.Translation.Y-=State.Hover*2.f;
		State.Widget->SetRenderTransform(Transform);
	}
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
		Start_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnStartButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnSettingButtonClicked);
	}
	if (IsValid(Exit_Btn))
	{
		Exit_Btn->OnClicked.AddUniqueDynamic(this, &UStartWidget::OnExitButtonClicked);
	}
}

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
		if (UButton* Button=Cast<UButton>(State.Widget)) Button->SetStyle(State.OriginalStyle);
	}
	ArtControls.Empty();
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
