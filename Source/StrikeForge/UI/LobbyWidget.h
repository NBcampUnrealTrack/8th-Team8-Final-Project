#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "LobbyWidget.generated.h"

class UButton;
class UWidget;
class UBackgroundBlur;
class UComboBoxString;
class USlider;

UCLASS()
class STRIKEFORGE_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnBackButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnStage1Clicked();
	UFUNCTION()
	void OnStage2Clicked();
	UFUNCTION()
	void OnStage3Clicked();
	UFUNCTION()
	void OnStage4Clicked();
	UFUNCTION()
	void OnStage5Clicked();
	UFUNCTION()
	void OnStage6Clicked();
	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnFullscreenModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void OpenStageLevel(int32 StageIndex);
	void SetSettingMenuVisible(bool bIsVisible);
	void InitializeSettingOptions();

protected:
	UPROPERTY(meta = (BindWidget))
	UButton* Stage1;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage2;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage3;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage4;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage5;
	UPROPERTY(meta = (BindWidget))
	UButton* Stage6;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Back_Btn;
	UPROPERTY(meta = (BindWidget))
	UWidget* SettingPanel;
	UPROPERTY(meta = (BindWidget))
	UBackgroundBlur* BackgroundBlur;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* DisplayComboBox;
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* FullScreenComboBox;

	UPROPERTY(EditDefaultsOnly, Category = "Stage")
	TArray<FName> StageRowNames = { TEXT("Stage1"), TEXT("Stage2"), TEXT("Stage3"), TEXT("Stage4"), TEXT("Stage5"), TEXT("Stage6") };
};