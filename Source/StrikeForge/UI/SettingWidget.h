// SettingWidget.h
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
UCLASS()
class STRIKEFORGE_API USettingWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	void OpenSetting();
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	UFUNCTION()
	void OnBackUiButtonClicked();
	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnBGMVolumeChanged(float Value);
	void InitializeSettingOptions();
	void CloseSetting();
protected:
	UPROPERTY(meta = (BindWidget))
	UButton* BackUi_Btn;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* DisplayComboBox;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* FullScreenComboBox;
	UPROPERTY(meta = (BindWidget))
	UBackgroundBlur* BackgroundBlur;
	UPROPERTY(meta = (BindWidget))
	USlider* BGMVolumeSlider;

	// 마스터 볼륨 조절용 - 에디터에서 지정 (예: SC_Master 사운드 클래스, 해당 SoundMix)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundMix* MasterSoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundClass* MasterSoundClass;

	float CurrentVolume = 1.0f;
};