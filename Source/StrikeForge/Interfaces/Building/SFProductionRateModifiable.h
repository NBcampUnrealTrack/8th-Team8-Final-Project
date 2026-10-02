#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFProductionRateModifiable.generated.h"

UINTERFACE(MinimalAPI)
class USFProductionRateModifiable : public UInterface
{
	GENERATED_BODY()
};

/** 생산속도 시너지의 대상이 되는 건물이 구현하는 인터페이스입니다. */
class STRIKEFORGE_API ISFProductionRateModifiable
{
	GENERATED_BODY()

public:
	/** 식별자별 생산속도 배율을 추가하거나 같은 식별자의 기존 값을 교체합니다. */
	virtual void SetProductionSpeedModifier(FName ModifierId, float InMultiplier) = 0;
	/** 지정한 출처의 생산속도 배율만 제거합니다. */
	virtual void RemoveProductionSpeedModifier(FName ModifierId) = 0;
};
