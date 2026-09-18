#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Animation/WidgetAnimationEvents.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFResourceGainWidget.generated.h"

class UImage;
class UTextBlock;
class UWidgetAnimation;

/**
 * 자원을 획득한 순간 잠깐 뜨는 배지(+8).
 *
 * 상태가 아니라 사건을 보여주므로 FSFOverheadStatus를 받지 않는다.
 * 획득할 때마다 Play()로 값을 새로 받고, 애니메이션이 끝나면 OnFinished를 쏜다.
 */
UCLASS()
class STRIKEFORGE_API USFResourceGainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 값을 채우고 애니메이션을 처음부터 재생한다.
	 * 재생 중에 다시 불리면 다시 시작한다. 숫자가 쌓이는 것보다 이쪽이 읽기 좋다.
	 */
	void Play(EResourceType InResourceType, int32 InAmount);

	/** 애니메이션이 끝났을 때. 판이 받아 컴포넌트에 올린다. */
	FSimpleMulticastDelegate OnFinished;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ResourceIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AmountText;

	/** WBP의 Animations 패널에서 만든 애니메이션. 이름이 GainAnim이어야 한다. */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> GainAnim;

	/**
	 * 자원 종류별 아이콘. WBP 기본값에서 지정한다.
	 *
	 * 자원 비주얼을 쓰는 곳이 여기뿐이라 아직 여기 둔다.
	 * 두 번째 사용처가 생기면 색(SFOverheadUIColors)과 함께 공용 DataAsset으로 옮긴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	TMap<EResourceType, TObjectPtr<UTexture2D>> ResourceIcons;

private:
	UFUNCTION()
	void HandleAnimationFinished();

	/** 애니메이션 종료 구독은 첫 재생 때 한 번만 한다. */
	bool bFinishedBound = false;
};
