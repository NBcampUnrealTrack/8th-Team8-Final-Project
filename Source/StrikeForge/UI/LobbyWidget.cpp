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
#include "SFAchievementsWidget.h"
#include "Styling/CoreStyle.h"
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
	UCanvasPanel* CampaignCanvas = Canvas ? Canvas : Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (CampaignCanvas && !AchievementsButton)
	{
		AchievementsButtonFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AchievementsButtonFrame"));
		AchievementsButtonFrame->SetBrushColor(FLinearColor(.66f,.48f,.22f));
		AchievementsButtonFrame->SetPadding(FMargin(3.f));
		AchievementsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AchievementsButton"));
		FButtonStyle Style = AchievementsButton->GetStyle();
		Style.Normal = Style.Hovered = Style.Pressed = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Style.Normal.TintColor = FLinearColor(.018f,.12f,.115f);
		Style.Hovered.TintColor = FLinearColor(.035f,.22f,.19f);
		Style.Pressed.TintColor = FLinearColor(.012f,.07f,.065f);
		AchievementsButton->SetStyle(Style);
		AchievementsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAchievementsButtonClicked);
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		int32 Completed = 0;
		const auto Entries = Progress ? Progress->GetPVEAchievements() : TArray<FSFPVEAchievement>();
		for (const auto& Entry : Entries) if (Entry.bUnlocked) ++Completed;
		Label->SetText(FText::Format(NSLOCTEXT("SFLobby", "Achievements", "업적   {0} / {1}"), FText::AsNumber(Completed), FText::AsNumber(Entries.Num())));
		Label->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font")), 28));
		Label->SetColorAndOpacity(FLinearColor(.98f,.87f,.65f));
		Label->SetJustification(ETextJustify::Center);
		AchievementsButton->SetContent(Label);
		AchievementsButtonFrame->SetContent(AchievementsButton);
		auto* ButtonSlot = CampaignCanvas->AddChildToCanvas(AchievementsButtonFrame);
		ButtonSlot->SetAnchors(FAnchors(.5f,0.f));
		ButtonSlot->SetAlignment(FVector2D(.5f,0.f));
		ButtonSlot->SetPosition(FVector2D(0.f,64.f));
		ButtonSlot->SetSize(FVector2D(248.f,72.f));
		ButtonSlot->SetZOrder(30);
		USFAudioLibrary::ApplyMenuButtonSounds(this);
	}
	if (CampaignCanvas && Progress && !ContinueCampaignButton)
	{
		const FName NextStage = Progress->GetNextCampaignStageRowName();
		ContinueCampaignButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueCampaignButton"));
		ContinueCampaignButton->SetBackgroundColor(FLinearColor(.035f,.19f,.17f,1.f));
		ContinueCampaignButton->SetIsEnabled(!NextStage.IsNone());
		ContinueCampaignButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnContinueCampaignClicked);
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(NextStage.IsNone()
			? NSLOCTEXT("SFLobby", "CampaignComplete", "캠페인 완료 - 모든 스테이지 클리어")
			: FText::Format(Progress->GetHighestClearedStageOrder() > 0
				? NSLOCTEXT("SFLobby", "ContinueCampaign", "이어하기 - STAGE {0}")
				: NSLOCTEXT("SFLobby", "StartCampaign", "캠페인 시작 - STAGE {0}"),
				FText::AsNumber(Progress->GetHighestClearedStageOrder() + 1)));
		Label->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font")), 24));
		Label->SetColorAndOpacity(FLinearColor(.96f,.83f,.55f));
		Label->SetJustification(ETextJustify::Center);
		ContinueCampaignButton->SetContent(Label);
		auto* ContinueSlot = CampaignCanvas->AddChildToCanvas(ContinueCampaignButton);
		ContinueSlot->SetAnchors(FAnchors(.5f, .85f));
		ContinueSlot->SetAlignment(FVector2D(.5f,.5f));
		ContinueSlot->SetPosition(FVector2D::ZeroVector);
		ContinueSlot->SetSize(FVector2D(500.f,64.f));
		ContinueSlot->SetZOrder(30);
		USFAudioLibrary::ApplyMenuButtonSounds(this);
	}
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
		NoticeSlot->SetPosition(FVector2D(0.f,154.f));
		NoticeSlot->SetSize(FVector2D(680.f,72.f));
		NoticeSlot->SetZOrder(30);
		USFAudioLibrary::ApplyMenuButtonSounds(this);
	}
}

void ULobbyWidget::NativeDestruct()
{
	if (AchievementsWidget)
	{
		AchievementsWidget->OnClosed.Clear();
		AchievementsWidget->RemoveFromParent();
	}
	AchievementsWidget = nullptr;
	if (AchievementsButtonFrame) AchievementsButtonFrame->RemoveFromParent();
	AchievementsButtonFrame = nullptr;
	AchievementsButton = nullptr;
	if (ContinueCampaignButton) ContinueCampaignButton->RemoveFromParent();
	ContinueCampaignButton = nullptr;
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
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
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
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	if (IsValid(SettingWidget))
	{
		SettingWidget->OpenSetting();
	}
}

void ULobbyWidget::OnStoryButtonClicked()
{
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
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

void ULobbyWidget::OnAchievementsButtonClicked()
{
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
	if (StoryWidget && StoryWidget->IsInViewport()) return;
	if (RaceSelectionWidget && RaceSelectionWidget->IsInViewport()) return;
	if (SettingWidget && SettingWidget->IsVisible()) return;
	// Rebuild from the saved campaign snapshot each time the journal opens.
	AchievementsWidget = CreateWidget<USFAchievementsWidget>(GetOwningPlayer());
	if (!AchievementsWidget) return;
	AchievementsWidget->OnClosed.AddWeakLambda(this, [this]()
	{
		if (AchievementsButton) AchievementsButton->SetFocus();
	});
	AchievementsWidget->AddToViewport(500);
}

void ULobbyWidget::OnStage1Clicked() { OpenStageLevel(0); }
void ULobbyWidget::OnStage2Clicked() { OpenStageLevel(1); }
void ULobbyWidget::OnStage3Clicked() { OpenStageLevel(2); }
void ULobbyWidget::OnStage4Clicked() { OpenStageLevel(3); }
void ULobbyWidget::OnStage5Clicked() { OpenStageLevel(4); }
void ULobbyWidget::OnStage6Clicked() { OpenStageLevel(5); }
void ULobbyWidget::OnStage7Clicked() { OpenStageLevel(6); }

void ULobbyWidget::OnContinueCampaignClicked()
{
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
	const USFGameInstance* Progress = GetGameInstance<USFGameInstance>();
	if (!Progress) return;
	const int32 NextIndex = StageRowNames.IndexOfByKey(Progress->GetNextCampaignStageRowName());
	if (NextIndex == INDEX_NONE) return;
	if (StagePresentation) StagePresentation->RequestLaunch(NextIndex);
	else OpenStageLevel(NextIndex);
}

void ULobbyWidget::OpenStageLevel(int32 StageIndex)
{
	if (AchievementsWidget && AchievementsWidget->IsInViewport()) return;
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
