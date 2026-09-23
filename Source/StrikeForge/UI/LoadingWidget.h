#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/GameState/SFGameState.h"
#include "LoadingWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UCommonVideoPlayer;
class UWidgetAnimation;

UCLASS()
class STRIKEFORGE_API ULoadingWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	UFUNCTION()
	void SetProgress(float InPercent01);
	UFUNCTION()
	void HandleStageStateChanged(ESFStageState InGameState);
	UFUNCTION(BlueprintCallable, Category = "SF|Loading")
	void FinishLoadingScreen();
	UFUNCTION(BlueprintCallable, Category = "SF|Loading")
	void SetStageTitle(const FText& InTitle);

protected:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* LoadingBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PercentText;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonVideoPlayer> BackgroundVideo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StageTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TipText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_TipFade;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_FadeIn;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_FadeOut;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Loop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading", meta = (AllowPrivateAccess = true, UIMin = 0.0f))
	float DelayStartTime = 1.f;

	UPROPERTY(EditAnywhere, Category = "Loading", meta = (UIMin = 0.1f))
	float ProgressInterpSpeed = 3.f;

	UPROPERTY(EditAnywhere, Category = "Loading", meta = (UIMin = 0.01f))
	float MinProgressSpeed = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Loading|Tips", meta = (MultiLine = true))
	TArray<FText> Tips;

	UPROPERTY(EditAnywhere, Category = "Loading|Tips")
	float TipInterval = 4.f;

	UPROPERTY()
	TObjectPtr<ASFGameState> CachedGameState;

	FTimerHandle FakeProgressTimerHandle;

private:
	void StartBackgroundVideo();
	void RetryPlayVideo();
	void StopBackgroundVideo();
	void ApplyDisplayedProgress();
	void BeginFadeOut();
	void ShowNextTip();

	UFUNCTION()
	void OnFadeOutFinished();

	FWidgetAnimationDynamicEvent FadeOutFinishedEvent;

	FTimerHandle TipTimerHandle;
	FTimerHandle VideoRetryTimerHandle;
	int32 VideoRetryCount = 0;
	int32 LastTipIndex = INDEX_NONE;

	float TargetProgress = 0.f;
	float DisplayedProgress = 0.f;
	bool bWaitingForFullBar = false;
	bool bFinishing = false;
};