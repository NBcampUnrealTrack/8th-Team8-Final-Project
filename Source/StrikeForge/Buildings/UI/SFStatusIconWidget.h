#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "SFStatusIconWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 상태 아이콘 하나가 어떻게 보일지.
 *
 * 고갈 아이콘은 새 그림을 만들지 않고 자원 아이콘(T_SFGoldIcon, T_SFEtherIcon)에 X를 겹친다.
 * 자원 아이콘이 바뀌면 고갈 아이콘도 그대로 따라간다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStatusIconVisual
{
	GENERATED_BODY()

	/** 아이콘 그림. 비워두면 아이콘을 접는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 그림 위에 X(CrossImage)를 겹칠지. 고갈 아이콘에서 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	bool bShowCross = false;
};

/**
 * 건물 머리 위 상태 아이콘(생산 대기·광산 고갈).
 *
 * 계속 떠 있는 상태를 그린다. 잠깐 떴다 사라지는 획득 배지(USFResourceGainWidget)와는 역할이 다르다.
 * 자기가 그릴 값(아이콘 종류)만 받는다. 값이 바뀌었는지는 판(USFOverheadPanelWidget)이 판단한다.
 */
UCLASS()
class STRIKEFORGE_API USFStatusIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 종류에 맞는 그림과 X 겹침을 적용한다. None이거나 그림이 지정되지 않았으면 둘 다 접는다. */
	void SetIcon(ESFOverheadStatusIcon InIcon);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	/**
	 * IconImage 위에 겹치는 X. 텍스처와 크기·위치는 WBP에서 정한다.
	 * 없으면 X 없이 그림만 보인다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CrossImage;

	/** 아이콘 종류별 겉모습. WBP 기본값에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	TMap<ESFOverheadStatusIcon, FSFStatusIconVisual> IconVisuals;
};
