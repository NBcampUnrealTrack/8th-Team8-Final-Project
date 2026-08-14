#include "StartWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Components/BackgroundBlur.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"

void UStartWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	// 버튼 클릭 이벤트 바인딩
	if (IsValid(Start_Btn))
	{
		Start_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnStartButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnSettingButtonClicked);
	}
	if (IsValid(Exit_Btn))
	{
		Exit_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnExitButtonClicked);
	}
	if (IsValid(BackUi_Btn))
	{
		BackUi_Btn->OnClicked.AddDynamic(this, &UStartWidget::OnBackUiButtonClicked);
	}

	// 옵션 초기화 (이벤트 바인딩 전에 호출해야 불필요한 OnSelectionChanged 방지)
	InitializeSettingOptions();

	// 콤보박스 선택 변경 이벤트 바인딩
	if (IsValid(DisplayComboBox))
	{
		DisplayComboBox->OnSelectionChanged.AddDynamic(this, &UStartWidget::OnResolutionChanged);
	}
	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->OnSelectionChanged.AddDynamic(this, &UStartWidget::OnFullscreenModeChanged);
	}

	// 초기 상태: 설정창 숨김
	SetSettingMenuVisible(false);
}

void UStartWidget::OnStartButtonClicked()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToLobby();
}

void UStartWidget::OnSettingButtonClicked()
{
	if (!IsValid(SettingPanel))
	{
		return;
	}

	bool bCurrentlyVisible = (SettingPanel->GetVisibility() == ESlateVisibility::Visible);
	SetSettingMenuVisible(!bCurrentlyVisible);
}

void UStartWidget::InitializeSettingOptions()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	// 1. 해상도 옵션 설정
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

	// 2. 전체화면 모드 옵션 설정
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

void UStartWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// 코드 내부 지정으로 인한 트리거 방지
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

void UStartWidget::OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
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

void UStartWidget::OnExitButtonClicked()
{
	APlayerController* SpecificPlayer = GetOwningPlayer();
	if (IsValid(SpecificPlayer))
	{
		UKismetSystemLibrary::QuitGame(this, SpecificPlayer, EQuitPreference::Quit, false);
	}
}

void UStartWidget::OnBackUiButtonClicked()
{
	SetSettingMenuVisible(false);
}

void UStartWidget::SetSettingMenuVisible(bool bIsVisible)
{
	ESlateVisibility SettingVis = bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	ESlateVisibility MainUIVis = bIsVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	if (IsValid(SettingPanel)) { SettingPanel->SetVisibility(SettingVis); }
	if (IsValid(BackgroundBlur)) { BackgroundBlur->SetVisibility(SettingVis); }
	if (IsValid(BackUi_Btn)) { BackUi_Btn->SetVisibility(SettingVis); }
	if (IsValid(Title)) { Title->SetVisibility(MainUIVis); }
	if (IsValid(Start_Btn)) { Start_Btn->SetVisibility(MainUIVis); }
	if (IsValid(Setting_Btn)) { Setting_Btn->SetVisibility(MainUIVis); }
	if (IsValid(Exit_Btn)) { Exit_Btn->SetVisibility(MainUIVis); }

	if (bIsVisible)
	{
		SetFocus();
	}
}

FReply UStartWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
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