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
static constexpr float DefaultMouseSensitivity = 1.0f;
void USettingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (IsValid(BackUi_Btn))
	{
		BackUi_Btn->OnClicked.AddDynamic(this, &USettingWidget::OnBackUiButtonClicked);
	}

	// 저장된 볼륨 불러오기 (마스터/BGM/효과음 각각)
	GConfig->GetFloat(TEXT("Audio"), TEXT("MasterVolume"), CurrentMasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Audio"), TEXT("BGMVolume"), CurrentBGMVolume, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Audio"), TEXT("SXVolume"), CurrentSXVolume, GGameUserSettingsIni);

	// 저장된 마우스 감도 불러오기 (없으면 기본값 사용)
	CurrentMouseSensitivity = DefaultMouseSensitivity;
	GConfig->GetFloat(TEXT("Input"), TEXT("MouseSensitivity"), CurrentMouseSensitivity, GGameUserSettingsIni);

	InitializeSettingOptions();

	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->OnSelectionChanged.AddDynamic(this, &USettingWidget::OnFullscreenModeChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &USettingWidget::OnMasterVolumeChanged);
		MasterVolumeSlider->SetValue(CurrentMasterVolume);
	}
	if (IsValid(BGMVolumeSlider))
	{
		BGMVolumeSlider->OnValueChanged.AddDynamic(this, &USettingWidget::OnBGMVolumeChanged);
		BGMVolumeSlider->SetValue(CurrentBGMVolume);
	}
	if (IsValid(SXVolumeSlider))
	{
		SXVolumeSlider->OnValueChanged.AddDynamic(this, &USettingWidget::OnSXVolumeChanged);
		SXVolumeSlider->SetValue(CurrentSXVolume);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->OnValueChanged.AddDynamic(this, &USettingWidget::OnMouseSensitivityChanged);
		MouseSensitivitySlider->SetValue(CurrentMouseSensitivity);
	}

	// 사운드 믹스를 게임 시작 시 한 번 push 해둬야 override가 먹음
	if (MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(GetWorld(), MasterSoundMix);
	}

	// 저장된 볼륨을 즉시 반영 (마스터/BGM/효과음)
	if (MasterSoundMix)
	{
		if (MasterSoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, MasterSoundClass, CurrentMasterVolume, 1.0f, 0.0f, true);
		}
		if (BGMSoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, BGMSoundClass, CurrentBGMVolume, 1.0f, 0.0f, true);
		}
		if (SXSoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, SXSoundClass, CurrentSXVolume, 1.0f, 0.0f, true);
		}
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

void USettingWidget::OnMasterVolumeChanged(float Value)
{
	CurrentMasterVolume = Value;

	if (MasterSoundMix && MasterSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, MasterSoundClass, Value, 1.0f, 0.0f, true);
	}

	GConfig->SetFloat(TEXT("Audio"), TEXT("MasterVolume"), Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USettingWidget::OnBGMVolumeChanged(float Value)
{
	CurrentBGMVolume = Value;

	if (MasterSoundMix && BGMSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, BGMSoundClass, Value, 1.0f, 0.0f, true);
	}

	GConfig->SetFloat(TEXT("Audio"), TEXT("BGMVolume"), Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USettingWidget::OnSXVolumeChanged(float Value)
{
	CurrentSXVolume = Value;

	if (MasterSoundMix && SXSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, SXSoundClass, Value, 1.0f, 0.0f, true);
	}

	GConfig->SetFloat(TEXT("Audio"), TEXT("SXVolume"), Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USettingWidget::OnMouseSensitivityChanged(float Value)
{
	CurrentMouseSensitivity = Value;

	// ini에 저장 - PlayerController/Character 쪽에서 같은 Section/Key로 읽어서 룩 입력에 곱해주면 됨
	// 예: GConfig->GetFloat(TEXT("Input"), TEXT("MouseSensitivity"), Sensitivity, GGameUserSettingsIni);
	GConfig->SetFloat(TEXT("Input"), TEXT("MouseSensitivity"), Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}