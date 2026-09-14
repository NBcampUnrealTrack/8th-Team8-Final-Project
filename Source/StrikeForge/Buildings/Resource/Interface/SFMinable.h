#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "UObject/Interface.h"
#include "SFMinable.generated.h"

class UMiningComponent;
class AResourceBase;

UINTERFACE()
class UMinable : public UInterface
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FMinableData
{
	GENERATED_BODY()
	// 현재의 추출량, 남은자원량, 채굴진행도 비
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ExtractAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResourceLeftAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GetMiningProgressRatio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EResourceType ResourceType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResources UpgradeCost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<UMiningComponent> MiningComponent;
	// << MiningComponent에 있는 알림 델리게이트 >>
	// 이번 타임에 자원을 캔 양				: OnResourceExtracted				-반환파라미터 (EResourceType ResourceType, int32 Amount)
	// 연결된 자원의 남은 양				: OnRemainingDepositChanged			-반환파라미터 (EResourceType ResourceType, int32 Remaining)
	// 채굴진행도 float 비율 0.0 ~ 1.0	: OnExtractProgressRatioChanged		-반환파라미터 (EResourceType ResourceType, float CurrentExtractRatio)
	// 고갈시							: OnResourceDepleted				-반환파라미터 없음
};

class STRIKEFORGE_API IMinable
{
	GENERATED_BODY()

public:
	/* 이 건물이 해당 자원을 사용할 수 있는지 검사한다.
	 * 이 함수에서는 상태를 변경하지 않는다. */
	virtual bool CanAcceptResource(const AResourceBase* InResource) const = 0;
	/* 호환되는 자원을 실제로 연결한다. */
	virtual bool TryLinkResource(AResourceBase* InResource) = 0;
	/* 연결된 자원이 그리드에서 제거될 때 호출한다. */
	virtual void UnlinkResource(const AResourceBase* InResource) = 0;
	
	// 광산 정보 Getter
	virtual const FMinableData GetMinableData() = 0;
};