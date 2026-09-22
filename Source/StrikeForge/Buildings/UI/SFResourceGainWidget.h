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
 * 자원 하나가 획득 배지에서 어떻게 보일지.
 *
 * 아이콘 맵과 색 맵을 따로 두면 자원을 추가할 때 한쪽만 채우고 넘어가기 쉽다.
 * 한 자원에 딸린 것은 한 자리에 모은다.
 *
 * 글자 채우기 색은 여기 없다. 그건 진행도 바와 같아야 하므로
 * SFOverheadUIColors::FromResourceType()이 정한다. 둘이 어긋나면 안 된다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFResourceGainVisual
{
	GENERATED_BODY()

	/** 배지에 띄울 아이콘. 비워두면 숫자만 나온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 숫자 테두리 색. 두께는 WBP의 AmountText가 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FLinearColor OutlineColor = FLinearColor::Black;
};

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
	 * 자원 종류별 겉모습. WBP 기본값에서 지정한다.
	 *
	 * 자원 비주얼을 쓰는 곳이 여기뿐이라 아직 여기 둔다.
	 * 두 번째 사용처가 생기면 색(SFOverheadUIColors)과 함께 공용 DataAsset으로 옮긴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	TMap<EResourceType, FSFResourceGainVisual> ResourceVisuals;

private:
	UFUNCTION()
	void HandleAnimationFinished();

	/** 애니메이션 종료 구독은 첫 재생 때 한 번만 한다. */
	bool bFinishedBound = false;

	/** WBP가 지정한 원래 테두리 색. 첫 재생 때 한 번 저장해 두고 되돌릴 때 쓴다. */
	FLinearColor DefaultOutlineColor = FLinearColor::Black;
	bool bDefaultOutlineCached = false;
};
