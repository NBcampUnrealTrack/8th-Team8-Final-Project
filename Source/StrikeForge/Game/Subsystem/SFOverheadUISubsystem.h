#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFOverheadUISubsystem.generated.h"

/**
 * 배율이 바뀌었다는 신호. 값은 싣지 않는다.
 *
 * 구독자는 필요할 때 GetUIScale()로 읽는다. 값을 실어 보내면 각자 복사본을 들게 되고,
 * 어느 한쪽이 갱신을 놓치는 순간 조용히 어긋난다.
 */
DECLARE_MULTICAST_DELEGATE(FSFOnOverheadUIScaleChanged);

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
	// Subsystem호출용
	static USFOverheadUISubsystem* Get(const UObject* WorldContext);

	/** 카메라가 줌을 바꿀 때 호출한다. */
	void SetUIScale(float InScale);
	
	// 초기값 대입용
	FORCEINLINE float GetUIScale() const { return CurrentUIScale; }

	/** 값이 실제로 바뀔 때만 호출된다. */
	FSFOnOverheadUIScaleChanged OnUIScaleChanged;

protected:
	// 월드서브시브템이 에디터에서 실행되는것을 방지
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		// 기본값에는 Editor도 포함된다. 게임에서만 필요하므로 제외한다.
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	float CurrentUIScale = 1.f;
};