// SettingWidget.cpp
#include "SettingWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/BackgroundBlur.h"
#include "Components/Slider.h"
#include "GameFramework/GameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

static const TCHAR* ClickSoundPath = TEXT("/Game/Audio/T_SFClick.T_SFClick");

void USettingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (IsValid(BackUi_Btn))
	{
		BackUi_Btn->OnClicked.AddDynamic(this, &USettingWidget::OnBackUiButtonClicked);
	}

	// 저장된 볼륨 불러오기
	GConfig->GetFloat(TEXT("Audio"), TEXT("MasterVolume"), CurrentVolume, GGameUserSettingsIni);

	InitializeSettingOptions();

	if (IsValid(DisplayComboBox))
	{
		DisplayComboBox->OnSelectionChanged.AddDynamic(this, &USettingWidget::OnResolutionChanged);
	}
	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->OnSelectionChanged.AddDynamic(this, &USettingWidget::OnFullscreenModeChanged);
	}
	if (IsValid(BGMVolumeSlider))
	{
		BGMVolumeSlider->OnValueChanged.AddDynamic(this, &USettingWidget::OnBGMVolumeChanged);
		BGMVolumeSlider->SetValue(CurrentVolume);
	}

	// 사운드 믹스를 게임 시작 시 한 번 push 해둬야 override가 먹음
	if (MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(GetWorld(), MasterSoundMix);
	}

	// 저장된 볼륨을 즉시 반영
	if (MasterSoundMix && MasterSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, MasterSoundClass, CurrentVolume, 1.0f, 0.0f, true);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void USettingWidget::OpenSetting()
{
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
}

void USettingWidget::OnBackUiButtonClicked()
{
	if (USoundBase* ClickSound = LoadObject<USoundBase>(nullptr, ClickSoundPath))
	{
		UGameplayStatics::PlaySound2D(this, ClickSound);
	}

	CloseSetting();
}

void USettingWidget::CloseSetting()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

FReply USettingWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseSetting();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

void USettingWidget::InitializeSettingOptions()
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

void USettingWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
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

void USettingWidget::OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
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

void USettingWidget::OnBGMVolumeChanged(float Value)
{
	CurrentVolume = Value;

	if (MasterSoundMix && MasterSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, MasterSoundClass, Value, 1.0f, 0.0f, true);
	}

	GConfig->SetFloat(TEXT("Audio"), TEXT("MasterVolume"), Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}