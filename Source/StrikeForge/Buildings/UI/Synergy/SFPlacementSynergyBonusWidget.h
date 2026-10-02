#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFPlacementSynergyBonusWidget.generated.h"

class UBuildingPlacement;
class UTextBlock;

/**
 * 건물 배치 모드 동안 커서 옆에, 배치 중인 건물이 지금 위치에서 받을 시너지 생산 보너스를 표시합니다.
 * 보너스가 0이어도 배치 모드 동안에는 보이고, 배치 모드가 끝나면 숨깁니다.
 */
UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFPlacementSynergyBonusWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 보너스를 "+40%" 형식으로 표시합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BonusText;

	/** 커서 위치에서 위젯 왼쪽 위까지의 화면 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Placement")
	FVector2D CursorOffset = FVector2D(24.f, 24.f);

private:
	/** 배치 모드 여부와 보너스를 받아 표시 여부와 텍스트를 갱신합니다. */
	UFUNCTION()
	void HandlePlacementSynergyBonusChanged(bool bPlacementActive, float BonusPercent);

	/** 커서 위치에 CursorOffset을 더한 곳으로 Canvas 슬롯을 옮깁니다. */
	void FollowCursor();

	UPROPERTY(Transient)
	TWeakObjectPtr<UBuildingPlacement> BoundPlacement;
};
