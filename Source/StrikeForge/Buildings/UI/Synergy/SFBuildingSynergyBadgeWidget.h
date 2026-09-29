#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFBuildingSynergyBadgeWidget.generated.h"

class UImage;
class UTextBlock;
struct FSFBuildingSynergyClusterProgressInfo;

/** 건물 시너지 클러스터의 다음 단계 진행도와 현재 생산 보너스를 표시하는 배지 위젯입니다. */
UCLASS(Blueprintable)
class STRIKEFORGE_API USFBuildingSynergyBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 클러스터 진행 정보를 읽어 아이콘·요구 건물 수·현재 보너스 표시를 한 번에 갱신합니다. */
	void SetProgressInfo(const FSFBuildingSynergyClusterProgressInfo& ProgressInfo);

	/** HUD가 월드 위치를 화면 좌표로 투영할 때 사용할 배지 기준 위치를 반환합니다. */
	const FVector& GetBadgeWorldLocation() const { return BadgeWorldLocation; }

protected:
	/** 시너지 Definition에 지정된 종족별 배지 아이콘입니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> BadgeIconImage = nullptr;

	/** 현재 클러스터 건물 수와 다음 단계 요구 수를 '현재 / 목표' 형태로 표시합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ProgressText = nullptr;

	/** 활성 단계의 생산 보너스를 표시하며, 아직 활성 단계가 없으면 숨깁니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BonusText = nullptr;

	/** 시너지 Definition의 DisplayName을 배지 제목으로 표시합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SynergyDisplayText = nullptr;

private:
	/** 클러스터 단계가 바뀔 때 계산된 월드 기준점으로, 실제 화면 좌표 변환은 HUD가 담당합니다. */
	UPROPERTY(Transient)
	FVector BadgeWorldLocation = FVector::ZeroVector;
};
