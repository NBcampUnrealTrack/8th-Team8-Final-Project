#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "StartWidget.generated.h"
class UButton;
class UWidget;
class UBackgroundBlur;
class UComboBoxString;
class UTextBlock;
UCLASS()
class STRIKEFORGE_API UStartWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnExitButtonClicked();
	UFUNCTION()
	void OnBackUiButtonClicked();
	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	void SetSettingMenuVisible(bool bIsVisible);
	void InitializeSettingOptions();
protected:
	UPROPERTY(meta = (BindWidget))
	UWidget* Title;
	UPROPERTY(meta = (BindWidget))
	UButton* Start_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* BackUi_Btn;
	UPROPERTY(meta = (BindWidget))
	UWidget* SettingPanel;
	UPROPERTY(meta = (BindWidget))
	UBackgroundBlur* BackgroundBlur;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* DisplayComboBox;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* FullScreenComboBox;
};