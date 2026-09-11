#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingWidget.h"
#include "StartWidget.generated.h"

class UButton;
class UWidget;
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

	UPROPERTY(meta = (BindWidget))
	USettingWidget* SettingWidget;
};