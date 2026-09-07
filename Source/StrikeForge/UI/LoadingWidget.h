#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UMediaPlayer;
class UMediaSource;

UCLASS()
class STRIKEFORGE_API ULoadingWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	void SetProgress(float InPercent01); // 0.0 ~ 1.0

protected:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* LoadingBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PercentText;

	// 배경 영상 깔릴 Image (Brush는 MediaTexture 참조하는 Material로 세팅)
	UPROPERTY(meta = (BindWidget))
	UImage* BackgroundImage;

	// 에디터에서 지정: 배경으로 재생할 mp4 소스
	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	UMediaSource* BackgroundVideoSource;

	// 에디터에서 지정: Media Player 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	UMediaPlayer* BackgroundMediaPlayer;

	FTimerHandle FakeProgressTimerHandle;
	float CurrentProgress = 0.f;

	void TickFakeProgress();
};