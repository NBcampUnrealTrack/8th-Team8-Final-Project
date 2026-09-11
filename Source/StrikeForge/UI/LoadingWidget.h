#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/GameState/SFGameState.h"
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
	// gamestate의 델리게이트에 연결되 데이터가 로드될때마다 gamestate에 값이 넘어가고
	// gamestate는 몇개가 로드됬는지 비율을 계산해 이 함수로 넘겨줌
	UFUNCTION()
	void SetProgress(float InPercent01); // 0.0 ~ 1.0
	UFUNCTION()
	void HandleStageStateChanged(ESFStageState InGameState);
	UFUNCTION(BlueprintCallable, Category = "SF|Loading")
	void FinishLoadingScreen();
protected:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* LoadingBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PercentText;

	// ��� ���� �� Image (Brush�� MediaTexture �����ϴ� Material�� ����)
	UPROPERTY(meta = (BindWidget))
	UImage* BackgroundImage;

	// �����Ϳ��� ����: ������� ����� mp4 �ҽ�
	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	UMediaSource* BackgroundVideoSource;

	// �����Ϳ��� ����: Media Player ����
	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	UMediaPlayer* BackgroundMediaPlayer;
	
	// Loading is too fast. Deliberately, slow down time using timer
	FTimerHandle FakeProgressTimerHandle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading", meta = (AllowPrivateAccess = true, UIMin = 0.0f))
	float DelayStartTime = 1.f;
	
	//caching class
	UPROPERTY()
	TObjectPtr<ASFGameState> CachedGameState;
};