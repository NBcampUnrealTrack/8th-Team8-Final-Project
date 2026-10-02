#pragma once

#include "CoreMinimal.h"
#include "SFBuildingSynergyTypes.generated.h"

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

/** 건물 Definition이 제공하는 시너지 규칙을 참조합니다. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFGrantedBuildingSynergy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy")
	TObjectPtr<USFBuildingSynergyDefinition> Definition = nullptr;
};
