#pragma once

#include "CoreMinimal.h"
#include "SFBuildingType.generated.h"

UENUM(BlueprintType)
enum class ESFBuildingType : uint8
{
	None UMETA(DisplayName = "선택되지 않음"),

	Nexus UMETA(DisplayName = "넥서스"),
	House UMETA(DisplayName = "주거 건물"),

	MineralMine UMETA(DisplayName = "미네랄 광산"),
	EtherMine UMETA(DisplayName = "에테르 광산"),

	Barracks UMETA(DisplayName = "일반 병영"),
	RangedProduction UMETA(DisplayName = "원거리 병영"),
	SpecialProduction UMETA(DisplayName = "특수 병영")
};