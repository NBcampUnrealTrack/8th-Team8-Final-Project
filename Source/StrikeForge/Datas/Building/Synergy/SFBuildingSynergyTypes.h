#pragma once

#include "CoreMinimal.h"
#include "SFBuildingSynergyTypes.generated.h"

class UNiagaraSystem;
class UTexture2D;
class USFBuildingSynergyDefinition;

/** 건물 수에 따라 선택되는 시너지 단계입니다. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingSynergyLevel
{
	GENERATED_BODY()

	/** 이 단계를 활성화하는 최소 원천 건물 수입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "1"))
	int32 MinSourceBuildingCount = 2;

	/** 원천 건물의 로컬 X/Y 기준 효과 구역 전체 셀 수이며, 건물 회전에 따라 함께 회전합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint EffectSizeCells = FIntPoint(5, 5);

	/** 2를 입력하면 기본 생산력에 2%가 추가됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0"))
	float ProductionBonusPercent = 25.f;
};

/**
 * 건물 Definition이 제공하는 시너지와 종족별 표현 설정입니다.
 * 규칙은 공용 Definition을 참조하고 Niagara·아이콘만 건물별로 다르게 지정합니다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFGrantedBuildingSynergy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy")
	TObjectPtr<USFBuildingSynergyDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Presentation")
	TSoftObjectPtr<UNiagaraSystem> BoundaryEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Presentation")
	FLinearColor BoundaryColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy|Presentation")
	TSoftObjectPtr<UTexture2D> BadgeIcon;
};
