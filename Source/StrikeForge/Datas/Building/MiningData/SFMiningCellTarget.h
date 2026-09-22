#pragma once

#include "CoreMinimal.h"
//#include "SFMiningCellTarget.generated.h"

class AResourceBase;

/*
 * 광산이 채굴할 자원 셀 하나를 나타냅니다.
 *
 * 같은 자원 액터의 서로 다른 셀은
 * 서로 다른 FSFMiningCellTarget이 됩니다.
 */
struct FSFMiningCellTarget
{
	TWeakObjectPtr<AResourceBase> ResourceActor;

	int32 ResourceCellIndex = INDEX_NONE;

	bool IsValid() const
	{
		return ResourceActor.IsValid() &&
			ResourceCellIndex >= 0;
	}
};