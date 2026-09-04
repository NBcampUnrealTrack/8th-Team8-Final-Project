#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFGridResourceConsumer.generated.h"

class AResourceBase;

UINTERFACE()
class USFGridResourceConsumer : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API ISFGridResourceConsumer
{
	GENERATED_BODY()

public:
	/**
	 * 이 건물이 해당 자원을 사용할 수 있는지 검사한다.
	 * 이 함수에서는 상태를 변경하지 않는다.
	 */
	virtual bool CanAcceptResource(
		const AResourceBase* InResource) const = 0;

	/**
	 * 호환되는 자원을 실제로 연결한다.
	 */
	virtual bool TryLinkResource(
		AResourceBase* InResource) = 0;

	/**
	 * 연결된 자원이 그리드에서 제거될 때 호출한다.
	 */
	virtual void UnlinkResource(
		const AResourceBase* InResource) = 0;
};