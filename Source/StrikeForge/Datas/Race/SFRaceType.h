#pragma once

#include "CoreMinimal.h"
#include "SFRaceType.generated.h"

UENUM(BlueprintType)
enum class ESFRace : uint8
{
	Human UMETA(DisplayName = "Human"),
	Animal UMETA(DisplayName = "Animal")
};