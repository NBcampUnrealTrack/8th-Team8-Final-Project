#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Input/Reply.h"
#include "SettingWidget.generated.h"

class UButton;
class UComboBoxString;
class UBackgroundBlur;
class USlider;
class USoundMix;
class USoundClass;
class UMouseSensitivitySubsystem;
class UFont;
class UFontFace;
class UCanvasPanel;
class UTextBlock;
class USFPauseMenuPresentation;
class USFOptionsPresentation;

UCLASS()
class STRIKEFORGE_API USettingWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
    USettingWidget(const FObjectInitializer& ObjectInitializer);
	void OpenSetting();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	UFUNCTION()
	void OnBackUiButtonClicked();
	UFUNCTION()
	void OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnMasterVolumeChanged(float Value);
	UFUNCTION()
	void OnBGMVolumeChanged(float Value);
	UFUNCTION()
	void OnSXVolumeChanged(float Value);
	UFUNCTION()
	void OnMouseSensitivityChanged(float Value);
	void InitializeSettingOptions();
	void CloseSetting();
    void BuildSettingsPresentation();
    void UpdateSettingsReadouts();
    UFUNCTION()
    UWidget* GenerateOptionLabel(FString Item);
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SettingsRoot;
    UPROPERTY(Transient) TObjectPtr<UFont> SettingsFont;
    UPROPERTY(Transient) TObjectPtr<UFontFace> SettingsBodyFace;
    UPROPERTY(Transient) TObjectPtr<UFontFace> SettingsTitleFace;
    UPROPERTY(Transient) TObjectPtr<UFont> SettingsTitleFont;
    UPROPERTY(Transient) TObjectPtr<USFPauseMenuPresentation> SettingsAtmosphere;
    UPROPERTY(Transient) TObjectPtr<USFOptionsPresentation> SettingsPanelArt;
    UPROPERTY(Transient) TObjectPtr<UWidget> SettingsContent;
    bool bSettingsOpening = false;
    UPROPERTY(Transient) TArray<TObjectPtr<USlider>> ReadoutSliders;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ValueLabels;
protected:
	UPROPERTY(meta = (BindWidget))
	UButton* BackUi_Btn;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* FullScreenComboBox;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* ResolutionComboBox;
	UPROPERTY(meta = (BindWidget))
	UBackgroundBlur* BackgroundBlur;
	UPROPERTY(meta = (BindWidget))
	USlider* MasterVolumeSlider;
	UPROPERTY(meta = (BindWidget))
	USlider* BGMVolumeSlider;
	UPROPERTY(meta = (BindWidget))
	USlider* SXVolumeSlider;
	UPROPERTY(meta = (BindWidget))
	USlider* MouseSensitivitySlider;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundMix* MasterSoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundClass* MasterSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundClass* BGMSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundClass* SXSoundClass;

	float CurrentMasterVolume = 1.0f;
	float CurrentBGMVolume = 1.0f;
	float CurrentSXVolume = 1.0f;
};
