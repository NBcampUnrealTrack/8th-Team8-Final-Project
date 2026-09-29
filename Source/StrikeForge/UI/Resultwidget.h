#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultWidget.generated.h"
class UButton;
class UWidget;
class UImage;
class UWidgetAnimation;
class USoundBase;

UCLASS()
class STRIKEFORGE_API UResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SF|Result")
	void SetResultState(bool bIsWin);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnExitButtonClicked();
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Win_Glow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Flash;

	// Names must match the animations in WBP_Win exactly
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Win;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_WinLoop;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_Lose;
	UPROPERTY(meta = (BindWidget))
	UWidget* Win_Image;
	UPROPERTY(meta = (BindWidget))
	UWidget* Loose_Image;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn_W;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn_F;
	UPROPERTY(EditAnywhere, Category = "SF|Result|Sound")
	TObjectPtr<USoundBase> WinSound;

	UPROPERTY(EditAnywhere, Category = "SF|Result|Sound")
	TObjectPtr<USoundBase> LoseSound;
private:
	UFUNCTION() void OnWinAnimFinished();
	FWidgetAnimationDynamicEvent WinAnimFinishedEvent;
	
};