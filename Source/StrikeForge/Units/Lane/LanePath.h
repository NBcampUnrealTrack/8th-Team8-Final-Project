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
 *
 * 여기에 더해 "코리도" 역할도 겸한다. 스플라인에서 LaneHalfWidth 안쪽이
 * 전투가 벌어져야 하는 길이고, 그 바깥은 유닛이 스스로 들어가지 않는 구역이다.
 * 자동 전투가 길 밖으로 흘러나가는 것을 막고(AUnitBase), 건물을 길 위에 세워
 * 진격로를 봉쇄하는 것을 막는(USFUnitSpawnLibrary::IsBuildableLocation) 판정의
 * 단일 출처다.
 *
 * 레벨에 LanePath 를 하나도 배치하지 않으면 두 제한 모두 자동으로 꺼진다 —
 * 코리도를 모르는 상태에서 유닛을 묶어두면 전부 그 자리에 굳기 때문이다.
 */
UCLASS()
class STRIKEFORGE_API ALanePath : public AActor
{
	GENERATED_BODY()

public:
	ALanePath();
	void SetTeamIndex(int32 InTeamIndex) { TeamIndex = InTeamIndex; }

	/**
	 * 진행도 0~1 지점의 월드 좌표.
	 * 0 = 스플라인 시작(아군 진영), 1 = 끝(적 진영).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FVector GetPointAtAlpha(float InAlpha) const;

	/** 이 위치가 라인 상 어디쯤인지(0~1) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	float GetAlphaAtLocation(const FVector& InWorldLocation) const;

	// ── 코리도 판정 ─────────────────────────────────────

	/**
	 * 이 지점이 코리도 밖으로 얼마나 벗어났는가(cm). 안에 있으면 0.
	 *
	 * 스플라인 위 최근접 점까지의 "수평" 거리에서 LaneHalfWidth 를 뺀 값이다.
	 * 수평으로 재는 이유는 캡슐 중심이 지면보다 90cm 높고 길에 경사도 있어서,
	 * 3D 거리로 재면 실제보다 넓게 벗어난 것으로 잡히기 때문이다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	float GetDistanceOutsideLane(const FVector& InWorldLocation) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	bool IsInsideLane(const FVector& InWorldLocation) const;

	/**
	 * 코리도로 돌아갈 목표 지점 — 스플라인 위 최근접 점.
	 *
	 * 코리도 가장자리가 아니라 한가운데를 노린다. 가장자리로 보내면 도착하자마자
	 * 다시 경계에 서게 되어, 분리력에 한 번만 밀려도 곧바로 또 이탈 판정이 난다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FVector GetClosestLanePoint(const FVector& InWorldLocation) const;

	/** 이 팀이 이 라인을 쓰는가. TeamIndex 가 -1 이면 모든 팀이 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	bool IsUsableByTeam(int32 InTeamIndex) const;

	/** 스플라인 전체 길이(cm). 거리를 알파로 환산할 때 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	float GetSplineLength() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FORCEINLINE float GetLaneHalfWidth() const { return LaneHalfWidth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	FORCEINLINE int32 GetTeamIndex() const { return TeamIndex; }

protected:
	// 코리도 판정의 출처인 만큼, 레벨에 놓였다는 사실을 서브시스템이 알아야 한다.
	// 전투 대상이 UCombatTargetComponent 로 스스로 등록하는 것과 같은 방식이다.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** sf.DrawLaneDebug 가 켜져 있을 때만 코리도를 그린다. 평소에는 즉시 반환한다 */
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> Spline;

private:
	/** 어느 팀이 이 라인을 타고 전진하는가. -1 이면 양쪽 다 사용 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true))
	int32 TeamIndex = INDEX_NONE;

	/**
	 * 라인 폭의 절반(cm). 웨이포인트 도달 판정과 코리도 판정에 함께 쓰인다.
	 *
	 * 흙길의 실제 반폭에 맞춘다. 좁게 잡으면 길 위에 서 있는 유닛까지 이탈로
	 * 판정되어 계속 한가운데로 몰리고, 넓게 잡으면 잔디까지 코리도에 들어와
	 * 제한이 사실상 없어진다. 에디터에서 sf.DrawLaneDebug 1 로 그려놓고
	 * 길 가장자리에 맞추는 것이 가장 빠르다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true, ClampMin = "50.0"))
	float LaneHalfWidth = 200.f;
};
