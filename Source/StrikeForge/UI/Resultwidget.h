#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultWidget.generated.h"
class UButton;
class UWidget;
class UImage;
class UWidgetAnimation;
class USFResultPresentation;


UCLASS()
class STRIKEFORGE_API UResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    UResultWidget(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintCallable, Category = "SF|Result")
	void SetResultState(bool bIsWin);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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
private:
	UPROPERTY(Transient) TObjectPtr<USFResultPresentation> Presentation;
	UPROPERTY(Transient) TObjectPtr<USFResultPresentation> ForegroundSparkles;
    UPROPERTY(Transient) TObjectPtr<USFResultPresentation> ExitDecoration;
	UPROPERTY(Transient) TObjectPtr<UButton> EtherPreviewExit;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> EtherPreviewAudio;
	UPROPERTY() TObjectPtr<class USoundWave> VictorySound;
    UPROPERTY() TObjectPtr<class USoundWave> DefeatSound;
    UPROPERTY() TObjectPtr<class UFont> VictoryFont;
    UPROPERTY() TObjectPtr<class UFontFace> ResultTitleFace;
    UPROPERTY(Transient) TObjectPtr<class UFont> ResultTitleFont;
	bool bEtherBurstPreview = false;
	bool bPreviewExitFocused = false;
	double ResultStartTime = 0.;
	bool bVictory = true;
	bool bPresenting = false;
	
};
