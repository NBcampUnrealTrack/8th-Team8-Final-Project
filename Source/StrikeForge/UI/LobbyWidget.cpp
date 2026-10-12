#include "LobbyWidget.h"
#include "Game/Audio/SFAudioLibrary.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"
#include "Datas/Stage/SFStageData.h"
#include "Engine/Texture2D.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Styling/SlateTypes.h"
#include "SFMenuAtmosphereWidget.h"
#include "SFLobbyStagePresentation.h"
#include "SFStoryWidget.h"
#include "SFRaceSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Migrate the serialized prototype binding as well as the native default.
	for (FName& Row : StageRowNames)
		if (Row == TEXT("StageHero") || Row == TEXT("Stage_Hero")) Row = TEXT("Stage05");
	if (IsValid(Story_Btn) && !bStoryBound)
	{
		// Replace the Blueprint's old story placeholder with the modal chronicle.
		OriginalStoryClick = Story_Btn->OnClicked;
		Story_Btn->OnClicked.Clear();
		Story_Btn->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStoryButtonClicked);
		bStoryBound = true;
	}

	if (IsValid(Back_Btn))
	{
		Back_Btn->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnBackButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnSettingButtonClicked);
	}
	if (IsValid(Stage1))
	{
		Stage1->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage1Clicked);
	}
	if (IsValid(Stage2))
	{
		Stage2->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage2Clicked);
	}
	if (IsValid(Stage3))
	{
		Stage3->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage3Clicked);
	}
	if (IsValid(Stage4))
	{
		Stage4->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage4Clicked);
	}
	if (IsValid(Stage5))
	{
		Stage5->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage5Clicked);
	}
	if (IsValid(Stage6))
	{
		Stage6->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage6Clicked);
	}
	if (IsValid(Stage6_1))
	{
		// Replace the legacy final-stage Blueprint shortcut with the same
		// progress-gated selection path used by the other six stages.
		Stage6_1->OnClicked.Clear();
		Stage6_1->OnClicked.AddUniqueDynamic(this, &ULobbyWidget::OnStage7Clicked);
	}

	// 로비 진입 시 스테이지 버튼들 잠금/클리어 상태 갱신
	RefreshStageButtons();
	USFAudioLibrary::ApplyMenuButtonSounds(this);

	// Keep the existing Blueprint controls and navigation; add presentation siblings.
	OriginalBackdrop = WidgetTree ? WidgetTree->FindWidget(TEXT("BackGround")) : nullptr;
	UCanvasPanel* Canvas = OriginalBackdrop ? Cast<UCanvasPanel>(OriginalBackdrop->GetParent()) : nullptr;
	UCanvasPanelSlot* OriginalSlot = OriginalBackdrop ? Cast<UCanvasPanelSlot>(OriginalBackdrop->Slot) : nullptr;
	if (Canvas && OriginalSlot && !AnimatedBackdrop)
	{
		AnimatedBackdrop = CreateWidget<USFMenuAtmosphereWidget>(GetOwningPlayer());
		if (AnimatedBackdrop && AnimatedBackdrop->IsReady())
		{
			UCanvasPanelSlot* BackdropSlot = Canvas->AddChildToCanvas(AnimatedBackdrop);
			BackdropSlot->SetLayout(OriginalSlot->GetLayout());
			BackdropSlot->SetZOrder(OriginalSlot->GetZOrder() - 1);
			OriginalBackdropVisibility = OriginalBackdrop->GetVisibility();
			OriginalBackdrop->SetVisibility(ESlateVisibility::Collapsed);
		}
		else AnimatedBackdrop = nullptr;
	}
	if (Canvas && AnimatedBackdrop && !StagePresentation)
	{
		StagePresentation = CreateWidget<USFLobbyStagePresentation>(GetOwningPlayer());
		UCanvasPanelSlot* PresentationSlot = Canvas->AddChildToCanvas(StagePresentation);
		PresentationSlot->SetAnchors(FAnchors(0.f,0.f,1.f,1.f));
		PresentationSlot->SetOffsets(FMargin(0.f));
		PresentationSlot->SetZOrder(-1);
		StagePresentation->InitializeControls(this,AnimatedBackdrop,StageRowNames);
	}
	const auto* Progress = GetGameInstance<USFGameInstance>();
	if (Canvas && Progress && Progress->HasUnreadStoryUnlock() && !StoryUnlockNotice)
	{
		StoryUnlockNotice = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StoryUnlockNotice"));
		StoryUnlockNotice->SetBrushColor(FLinearColor(.025f,.14f,.13f,.97f));
		StoryUnlockNotice->SetPadding(FMargin(12.f));
		auto* Button = WidgetTree->ConstructWidget<UButton>();
		auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(NSLOCTEXT("SFLobby", "StoryUnlocked", "새로운 이야기가 해금되었습니다   스토리 보기"));
		Text->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font")), 22));
		Text->SetColorAndOpacity(FLinearColor(.96f,.83f,.55f));
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		Button->SetBackgroundColor(FLinearColor::Transparent);
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::OnStoryButtonClicked);
		StoryUnlockNotice->SetContent(Button);
		auto* NoticeSlot = Canvas->AddChildToCanvas(StoryUnlockNotice);
		NoticeSlot->SetAnchors(FAnchors(.5f,0.f));
		NoticeSlot->SetAlignment(FVector2D(.5f,0.f));
		NoticeSlot->SetPosition(FVector2D(0.f,105.f));
		NoticeSlot->SetSize(FVector2D(680.f,72.f));
		NoticeSlot->SetZOrder(30);
		USFAudioLibrary::ApplyMenuButtonSounds(this);
	}
}

void ULobbyWidget::NativeDestruct()
{
	if (StoryUnlockNotice) StoryUnlockNotice->RemoveFromParent();
	StoryUnlockNotice = nullptr;
	if (RaceSelectionWidget) RaceSelectionWidget->RemoveFromParent();
	RaceSelectionWidget = nullptr;
	if (StoryWidget)
	{
		StoryWidget->OnClosed.Clear();
		StoryWidget->RemoveFromParent();
		StoryWidget = nullptr;
	}
	if (Story_Btn && bStoryBound) Story_Btn->OnClicked = OriginalStoryClick;
	bStoryBound = false;
	if (StagePresentation)
	{
		StagePresentation->RestoreControls();
		StagePresentation->RemoveFromParent();
	}
	StagePresentation = nullptr;
	if (AnimatedBackdrop) AnimatedBackdrop->RemoveFromParent();
	AnimatedBackdrop = nullptr;
	if (OriginalBackdrop) OriginalBackdrop->SetVisibility(OriginalBackdropVisibility);
	OriginalBackdrop = nullptr;
	Super::NativeDestruct();
}

void ULobbyWidget::OnBackButtonClicked()
{
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToTitle();
}

void ULobbyWidget::OnSettingButtonClicked()
{
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	if (IsValid(SettingWidget))
	{
		SettingWidget->OpenSetting();
	}
}

void ULobbyWidget::OnStoryButtonClicked()
{
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	if (SettingWidget && SettingWidget->IsVisible()) return;
	if (!StoryWidget)
	{
		StoryWidget = CreateWidget<USFStoryWidget>(GetOwningPlayer());
		if (!StoryWidget) return;
		StoryWidget->OnClosed.AddWeakLambda(this, [this]()
		{
			if (StoryWidget) StoryWidget->RemoveFromParent();
			if (Story_Btn) Story_Btn->SetFocus();
		});
	}
	StoryWidget->AddToViewport(500);
	StoryWidget->OpenStory();
	if (auto* Progress = GetGameInstance<USFGameInstance>()) Progress->MarkStoryUnlockRead();
	if (StoryUnlockNotice) StoryUnlockNotice->RemoveFromParent();
	StoryUnlockNotice = nullptr;
}

void ULobbyWidget::OnStage1Clicked() { OpenStageLevel(0); }
void ULobbyWidget::OnStage2Clicked() { OpenStageLevel(1); }
void ULobbyWidget::OnStage3Clicked() { OpenStageLevel(2); }
void ULobbyWidget::OnStage4Clicked() { OpenStageLevel(3); }
void ULobbyWidget::OnStage5Clicked() { OpenStageLevel(4); }
void ULobbyWidget::OnStage6Clicked() { OpenStageLevel(5); }
void ULobbyWidget::OnStage7Clicked() { OpenStageLevel(6); }

void ULobbyWidget::OpenStageLevel(int32 StageIndex)
{
	if (RaceSelectionWidget && RaceSelectionWidget->IsInViewport()) return;
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	if (!StageRowNames.IsValidIndex(StageIndex))
	{
		return;
	}

	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (!GameInstance->SelectStage(StageRowNames[StageIndex]))
	{
		return;
	}

	if (GameInstance->GetSelectedRace() == ESFRace::None)
	{
		if (!RaceSelectionWidget) RaceSelectionWidget = CreateWidget<USFRaceSelectionWidget>(GetOwningPlayer());
		if (RaceSelectionWidget) RaceSelectionWidget->AddToViewport(600);
		return;
	}

	GameInstance->MoveToStage();
}

// 26.08.11 추가
void ULobbyWidget::RefreshStageButtons()
{
	const TArray<UButton*> StageButtons =
	{
		Stage1,
		Stage2,
		Stage3,
		Stage4,
		Stage5,
		Stage6,
		Stage6_1
	};

	const int32 StageCount =
		FMath::Min(StageButtons.Num(), StageRowNames.Num());

	for (int32 StageIndex = 0;
		StageIndex < StageCount;
		++StageIndex)
	{
		RefreshStageButton(
			StageButtons[StageIndex],
			StageRowNames[StageIndex]
		);
	}
}

void ULobbyWidget::RefreshStageButton(UButton* StageButton, FName StageRowName)
{
	if (!IsValid(StageButton) || StageRowName.IsNone())
	{
		return;
	}

	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;

	if (!IsValid(GameInstance))
	{
		return;
	}

	USFDataTableManager* DataTableManager = GameInstance->GetSubsystem<USFDataTableManager>();

	if (!IsValid(DataTableManager))
	{
		return;
	}

	const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, StageRowName, TEXT("RefreshStageButton"));

	if (!StageData)
	{
		return;
	}

	const ESFStageProgressState StageState = GameInstance->GetStageProgressState(StageRowName);
	const TSoftObjectPtr<UTexture2D>* SelectedImage = nullptr;

	switch (StageState)
	{
	case ESFStageProgressState::Locked:
		SelectedImage = &StageData->LockedImage;
		StageButton->SetIsEnabled(false);
		break;

	case ESFStageProgressState::Unlocked:
		SelectedImage = &StageData->UnlockedImage;
		StageButton->SetIsEnabled(true);
		break;

	case ESFStageProgressState::Cleared:
		SelectedImage = &StageData->ClearedImage;
		StageButton->SetIsEnabled(true);
		break;

	default:
		StageButton->SetIsEnabled(false);
		return;
	}

	if (!SelectedImage || SelectedImage->IsNull())
	{
		return;
	}

	UTexture2D* StageTexture = SelectedImage->LoadSynchronous();

	if (!IsValid(StageTexture))
	{
		return;
	}

	ApplyStageButtonTexture(StageButton, StageTexture);
}

void ULobbyWidget::ApplyStageButtonTexture(UButton* StageButton, UTexture2D* StageTexture)
{
	if (!IsValid(StageButton) || !IsValid(StageTexture))
	{
		return;
	}

	FButtonStyle ButtonStyle = StageButton->GetStyle();

	ButtonStyle.Normal.SetResourceObject(StageTexture);
	ButtonStyle.Hovered.SetResourceObject(StageTexture);
	ButtonStyle.Pressed.SetResourceObject(StageTexture);
	ButtonStyle.Disabled.SetResourceObject(StageTexture);

	StageButton->SetStyle(ButtonStyle);
}
// 여기까지
