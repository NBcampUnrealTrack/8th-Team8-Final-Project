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

	UPROPERTY(meta = (BindWidget))
	class UWidget* Title;

	UPROPERTY(meta = (BindWidget))
	class UButton* Start_Btn;

	UPROPERTY(meta = (BindWidget))
	class UButton* Setting_Btn;

	UPROPERTY(meta = (BindWidget))
	class UButton* Exit_Btn;

	UPROPERTY(meta = (BindWidget))
	class UWidget* SettingPanel;

	UPROPERTY(meta = (BindWidget))
	class UBackgroundBlur* BackgroundBlur;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* DisplayComboBox;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* FullScreenComboBox;

	UFUNCTION()
	void OnStartButtonClicked();

	UFUNCTION()
	void OnSettingButtonClicked();

	UFUNCTION()
	void OnExitButtonClicked();

	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void SetSettingMenuVisible(bool bIsVisible);
	void InitializeSettingOptions();
};