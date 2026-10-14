#pragma once

#include "CoreMinimal.h"
#include "SFRaceType.generated.h"

UENUM(BlueprintType)
enum class ESFRace : uint8
{
	Human = 0 UMETA(DisplayName = "Human"),
	Beast = 1 UMETA(DisplayName = "Beast"),
	/** 아직 종족이 선택되지 않은 상태 */
	None = 255 UMETA(DisplayName = "None"),
};