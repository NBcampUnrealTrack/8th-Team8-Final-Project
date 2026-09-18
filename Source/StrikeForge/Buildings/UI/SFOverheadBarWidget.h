#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Blueprint/UserWidget.h"
#include "SFOverheadBarWidget.generated.h"

/**
 * 액터 머리 위에 뜨는 바 하나의 공통 부모.
 *
 * 상태를 받아 자기가 쓰는 필드만 읽어 그린다.
 * 값의 출처는 모른다. USFOverheadUIComponent가 타입을 몰라도 갱신할 수 있게 해준다.
 */
UCLASS(Abstract)
class STRIKEFORGE_API USFOverheadBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void ApplyStatus(const FSFOverheadStatus& InStatus) {}
};
