// SettingWidget.h
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "SettingWidget.generated.h"
class UButton;
class UComboBoxString;
class UBackgroundBlur;
class USlider;

UCLASS()
class STRIKEFORGE_API USettingWidget : public UUserWidget
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
};