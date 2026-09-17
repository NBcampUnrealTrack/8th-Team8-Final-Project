#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFOverheadUISubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FSFOnOverheadUIScaleChanged, float);

/**
 * 건물·유닛 머리 위 UI가 공유하는 상태를 보관한다.
 *
 * 지금은 카메라 줌 배율만 다루지만, 앞으로 표시 임계값이나 공통 스타일처럼
 * 오버헤드 UI 전체에 걸리는 값이 생기면 여기에 모은다.
 */
UCLASS()
class STRIKEFORGE_API USFOverheadUISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USFOverheadUISubsystem* Get(const UObject* WorldContext);

	/** 카메라가 줌을 바꿀 때 호출한다. */
	void SetUIScale(float InScale);

	FORCEINLINE float GetUIScale() const { return CurrentUIScale; }

	/** 값이 실제로 바뀔 때만 호출된다. */
	FSFOnOverheadUIScaleChanged OnUIScaleChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		// 기본값에는 Editor도 포함된다. 게임에서만 필요하므로 제외한다.
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	float CurrentUIScale = 1.f;
};