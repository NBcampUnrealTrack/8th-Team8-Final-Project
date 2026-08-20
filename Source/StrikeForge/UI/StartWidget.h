#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartWidget.generated.h"
class UButton;
class UWidget;
class USettingWidget;
UCLASS()
class STRIKEFORGE_API UStartWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnExitButtonClicked();

protected:
	UPROPERTY(meta = (BindWidget))
	UWidget* Title;
	UPROPERTY(meta = (BindWidget))
	UButton* Start_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn;

	// WBP_Start 캔버스에 직접 배치된 WBP_Setting 인스턴스 (Visibility 토글로 열고 닫음)
	UPROPERTY(meta = (BindWidget))
	USettingWidget* SettingWidget;
};