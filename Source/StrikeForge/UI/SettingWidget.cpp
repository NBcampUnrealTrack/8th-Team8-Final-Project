#include "SettingWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/BackgroundBlur.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/GameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "UI/MouseSensitivitySubsystem.h"

static const TCHAR* ClickSoundPath = TEXT("/Game/Sound/INUI_soundEffect/Button/T_SFClick.T_SFClick");

void USettingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (IsValid(BackUi_Btn))
	{
		BackUi_Btn->OnClicked.AddUniqueDynamic(this, &USettingWidget::OnBackUiButtonClicked);
		// The caption is baked into the panel art, outside the icon button.
		// Cover the caption with a transparent button using the same close action.
		if (auto* Canvas = Cast<UCanvasPanel>(BackUi_Btn->GetParent()))
		{
			if (auto* IconSlot = Cast<UCanvasPanelSlot>(BackUi_Btn->Slot))
			{
				auto* CaptionButton = Cast<UButton>(GetWidgetFromName(TEXT("BackCaptionHitArea")));
				if (!CaptionButton)
				{
					CaptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackCaptionHitArea"));
					FButtonStyle Style = BackUi_Btn->GetStyle();
					Style.Normal.DrawAs = Style.Hovered.DrawAs = Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
					CaptionButton->SetStyle(Style);
					UCanvasPanelSlot* CaptionSlot = Canvas->AddChildToCanvas(CaptionButton);
					CaptionSlot->SetLayout(IconSlot->GetLayout());
					CaptionSlot->SetPosition(IconSlot->GetPosition() + FVector2D(IconSlot->GetSize().X * .75f, 30.f));
					CaptionSlot->SetSize(FVector2D(200.f, 95.f));
					CaptionSlot->SetZOrder(IconSlot->GetZOrder() + 1);
				}
				CaptionButton->OnClicked.AddUniqueDynamic(this, &USettingWidget::OnBackUiButtonClicked);
			}
		}
	}

	GConfig->GetFloat(TEXT("Audio"), TEXT("MasterVolume"), CurrentMasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Audio"), TEXT("BGMVolume"), CurrentBGMVolume, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Audio"), TEXT("SXVolume"), CurrentSXVolume, GGameUserSettingsIni);

	InitializeSettingOptions();

	if (IsValid(FullScreenComboBox))
	{
		FullScreenComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingWidget::OnFullscreenModeChanged);
	}
	if (IsValid(ResolutionComboBox))
	{
		ResolutionComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingWidget::OnResolutionChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingWidget::OnMasterVolumeChanged);
		MasterVolumeSlider->SetValue(CurrentMasterVolume);
	}
	if (IsValid(BGMVolumeSlider))
	{
		BGMVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingWidget::OnBGMVolumeChanged);
		BGMVolumeSlider->SetValue(CurrentBGMVolume);
	}
	if (IsValid(SXVolumeSlider))
	{
		SXVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingWidget::OnSXVolumeChanged);
		SXVolumeSlider->SetValue(CurrentSXVolume);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->SetMinValue(0.1f);
		MouseSensitivitySlider->SetMaxValue(3.f);
		if (auto* Sensitivity = GetGameInstance()->GetSubsystem<UMouseSensitivitySubsystem>())
			MouseSensitivitySlider->SetValue(Sensitivity->GetMouseSensitivity());
		MouseSensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &USettingWidget::OnMouseSensitivityChanged);
	}

	if (MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(GetWorld(), MasterSoundMix);
	}

	if (MasterSoundMix)
	{
		if (MasterSoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(GetWorld(), MasterSoundMix, MasterSoundClass, 1.0f, 1.0f, 0.0f, true);
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

	if (IsValid(ResolutionComboBox))
	{
		ResolutionComboBox->ClearOptions();

		TArray<FIntPoint> SupportedResolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);

		TArray<FString> AddedLabels;
		for (const FIntPoint& Res : SupportedResolutions)
		{
			const FString Label = FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
			if (!AddedLabels.Contains(Label))
			{
				ResolutionComboBox->AddOption(Label);
				AddedLabels.Add(Label);
			}
		}

		const FIntPoint CurrentRes = Settings->GetScreenResolution();
		const FString CurrentLabel = FString::Printf(TEXT("%dx%d"), CurrentRes.X, CurrentRes.Y);

		if (AddedLabels.Contains(CurrentLabel))
		{
			ResolutionComboBox->SetSelectedOption(CurrentLabel);
		}
		else if (AddedLabels.Num() > 0)
		{
			ResolutionComboBox->SetSelectedOption(AddedLabels[0]);
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
	else
	{
		return;
	}
	if (Settings->GetFullscreenMode() == NewMode) return;

	Settings->SetFullscreenMode(NewMode);
	// UE 5.5 ApplySettings recreates every component's render state. A resize
	// during that empty-scene interval can assert in Nanite's primitive filter.
	// This control changes display mode only; leave scalability/render state alone.
	Settings->ApplyResolutionSettings(false);
	Settings->SaveSettings();
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
	if (!SelectedItem.Split(TEXT("x"), &WidthStr, &HeightStr))
	{
		return;
	}

	const int32 Width = FCString::Atoi(*WidthStr);
	const int32 Height = FCString::Atoi(*HeightStr);

	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	const FIntPoint Resolution(Width, Height);
	if (Settings->GetScreenResolution() == Resolution) return;
	Settings->SetScreenResolution(Resolution);
	// Do not reapply graphics quality or recreate scene primitives for a resize.
	Settings->ApplyResolutionSettings(false);
	Settings->SaveSettings();
}

void USettingWidget::OnMasterVolumeChanged(float Value)
{
	CurrentMasterVolume = Value;

	if (auto* Settings = GetGameInstance()->GetSubsystem<UMouseSensitivitySubsystem>())
		Settings->SetMasterVolume(Value);

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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMouseSensitivitySubsystem* MouseSensSubsystem = GI->GetSubsystem<UMouseSensitivitySubsystem>())
		{
			MouseSensSubsystem->SetMouseSensitivity(Value);
		}
	}
}
