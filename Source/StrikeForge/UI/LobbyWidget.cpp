#include "LobbyWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Components/BackgroundBlur.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"
#include "Datas/Stage/SFStageData.h"
#include "Engine/Texture2D.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Styling/SlateTypes.h"

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (IsValid(Back_Btn))
	{
		Back_Btn->OnClicked.AddDynamic(this, &ULobbyWidget::OnBackButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddDynamic(this, &ULobbyWidget::OnSettingButtonClicked);
	}
	if (IsValid(BackUi_Btn))
	{
		BackUi_Btn->OnClicked.AddDynamic(this, &ULobbyWidget::OnBackUiButtonClicked);
	}
	if (IsValid(Stage1))
	{
		Stage1->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage1Clicked);
	}
	if (IsValid(Stage2))
	{
		Stage2->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage2Clicked);
	}
	if (IsValid(Stage3))
	{
		Stage3->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage3Clicked);
	}
	if (IsValid(Stage4))
	{
		Stage4->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage4Clicked);
	}
	if (IsValid(Stage5))
	{
		Stage5->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage5Clicked);
	}
	if (IsValid(Stage6))
	{
		Stage6->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage6Clicked);
	}

	// 옵션 초기화 (이벤트 바인딩 전에 호출해야 불필요한 OnSelectionChanged 방지)
	InitializeSettingOptions();

	if (IsValid(DisplayComboBox))
	{
		DisplayComboBox->OnSelectionChanged.AddDynamic(this, &ULobbyWidget::OnResolutionChanged);
	}
	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->OnSelectionChanged.AddDynamic(this, &ULobbyWidget::OnFullscreenModeChanged);
	}

	// 초기 상태: 설정창 숨김
	SetSettingMenuVisible(false);

	// 26.08.13 추가(로비 버튼 갱신 함수)
	RefreshStageButtons();
}

void ULobbyWidget::OnBackButtonClicked()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToTitle();
}

void ULobbyWidget::OnSettingButtonClicked()
{
	if (!IsValid(SettingPanel))
	{
		return;
	}

	const bool bCurrentlyVisible = (SettingPanel->GetVisibility() == ESlateVisibility::Visible);
	SetSettingMenuVisible(!bCurrentlyVisible);
}

void ULobbyWidget::OnBackUiButtonClicked()
{
	SetSettingMenuVisible(false);
}

void ULobbyWidget::OnStage1Clicked() { OpenStageLevel(0); }
void ULobbyWidget::OnStage2Clicked() { OpenStageLevel(1); }
void ULobbyWidget::OnStage3Clicked() { OpenStageLevel(2); }
void ULobbyWidget::OnStage4Clicked() { OpenStageLevel(3); }
void ULobbyWidget::OnStage5Clicked() { OpenStageLevel(4); }
void ULobbyWidget::OnStage6Clicked() { OpenStageLevel(5); }

void ULobbyWidget::OpenStageLevel(int32 StageIndex)
{
	if (!StageRowNames.IsValidIndex(StageIndex))
	{
		return;
	}

	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (GameInstance->SelectStage(StageRowNames[StageIndex]))
	{
		GameInstance->MoveToStage();
	}
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
		Stage6
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

void ULobbyWidget::SetSettingMenuVisible(bool bIsVisible)
{
	const ESlateVisibility SettingVis = bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (IsValid(SettingPanel)) { SettingPanel->SetVisibility(SettingVis); }
	if (IsValid(BackgroundBlur)) { BackgroundBlur->SetVisibility(SettingVis); }
	if (IsValid(BackUi_Btn)) { BackUi_Btn->SetVisibility(SettingVis); }

	if (bIsVisible)
	{
		SetFocus();
	}
}

FReply ULobbyWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (IsValid(SettingPanel) && SettingPanel->GetVisibility() == ESlateVisibility::Visible)
		{
			SetSettingMenuVisible(false);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

void ULobbyWidget::InitializeSettingOptions()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	if (IsValid(DisplayComboBox))
	{
		DisplayComboBox->ClearOptions();
		DisplayComboBox->AddOption(TEXT("1024x576"));
		DisplayComboBox->AddOption(TEXT("1280x720"));
		DisplayComboBox->AddOption(TEXT("1366x768"));
		DisplayComboBox->AddOption(TEXT("1600x900"));
		DisplayComboBox->AddOption(TEXT("1920x1080"));
		DisplayComboBox->AddOption(TEXT("2560x1440"));

		const FIntPoint CurrentRes = Settings->GetScreenResolution();
		const FString CurrentResStr = FString::Printf(TEXT("%dx%d"), CurrentRes.X, CurrentRes.Y);

		if (DisplayComboBox->FindOptionIndex(CurrentResStr) != INDEX_NONE)
		{
			DisplayComboBox->SetSelectedOption(CurrentResStr);
		}
		else
		{
			DisplayComboBox->SetSelectedOption(TEXT("1920x1080"));
		}
	}

	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->ClearOptions();
		FullScreenComboBox->AddOption(TEXT("Fullscreen"));
		FullScreenComboBox->AddOption(TEXT("WindowedFullscreen"));
		FullScreenComboBox->AddOption(TEXT("Windowed"));

		switch (Settings->GetFullscreenMode())
		{
		case EWindowMode::Fullscreen:
			FullScreenComboBox->SetSelectedOption(TEXT("Fullscreen"));
			break;
		case EWindowMode::WindowedFullscreen:
			FullScreenComboBox->SetSelectedOption(TEXT("WindowedFullscreen"));
			break;
		case EWindowMode::Windowed:
		default:
			FullScreenComboBox->SetSelectedOption(TEXT("Windowed"));
			break;
		}
	}
}

void ULobbyWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	FString WidthStr, HeightStr;
	if (SelectedItem.Split(TEXT("x"), &WidthStr, &HeightStr))
	{
		const int32 Width = FCString::Atoi(*WidthStr);
		const int32 Height = FCString::Atoi(*HeightStr);

		Settings->SetScreenResolution(FIntPoint(Width, Height));
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
}

void ULobbyWidget::OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	EWindowMode::Type NewMode = EWindowMode::Windowed;

	if (SelectedItem == TEXT("Fullscreen"))
	{
		NewMode = EWindowMode::Fullscreen;
	}
	else if (SelectedItem == TEXT("WindowedFullscreen"))
	{
		NewMode = EWindowMode::WindowedFullscreen;
	}
	else if (SelectedItem == TEXT("Windowed"))
	{
		NewMode = EWindowMode::Windowed;
	}

	Settings->SetFullscreenMode(NewMode);
	Settings->ApplySettings(false);
	Settings->SaveSettings();
}