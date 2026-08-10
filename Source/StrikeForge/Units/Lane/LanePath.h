#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LanePath.generated.h"

class USplineComponent;

/**
 * 진격로. 스플라인 하나가 한 갈래 길을 나타낸다.
 *
 * 라인은 "대략적인 진격 방향"일 뿐이고 벽 회피는 NavMesh 가 맡는다.
 * 통로가 하나뿐인 맵에서 유닛이 한 점으로 몰려 병목이 생기는 것을 막는 용도다.
 */
UCLASS()
class STRIKEFORGE_API ALanePath : public AActor
{
	GENERATED_BODY()

public:
	ALanePath();

	/**
	 * 진행도 0~1 지점의 월드 좌표.
	 * 0 = 스플라인 시작(아군 진영), 1 = 끝(적 진영).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FVector GetPointAtAlpha(float InAlpha) const;

	/** 이 위치가 라인 상 어디쯤인지(0~1) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	float GetAlphaAtLocation(const FVector& InWorldLocation) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FORCEINLINE float GetLaneHalfWidth() const { return LaneHalfWidth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FORCEINLINE int32 GetTeamIndex() const { return TeamIndex; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;

private:
	/** 어느 팀이 이 라인을 타고 전진하는가. -1 이면 양쪽 다 사용 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true))
	int32 TeamIndex = INDEX_NONE;

	/** 라인 폭의 절반(cm). 웨이포인트 도달 판정에 쓰인다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true, ClampMin = "50.0"))
	float LaneHalfWidth = 200.f;
};
