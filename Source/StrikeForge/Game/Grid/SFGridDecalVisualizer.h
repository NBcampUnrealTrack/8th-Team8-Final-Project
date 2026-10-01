#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Interval.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "SFGridDecalVisualizer.generated.h"

class APlayerController;
class ASFBaseBuilding;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class USFBuildingSynergyDefinition;
struct FSFBuildingSynergyZoneInfo;

/** 셀 상태 텍스처 1장으로 여러 칸을 한 번에 그리는 데칼 한 층입니다. */
USTRUCT()
struct FSFGridMaskDecalLayer
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY()
	TObjectPtr<UTexture2D> Texture;
};

/**
 * 건물 배치 모드와 시너지 완성 격자를 데칼로 표시하는 로컬 전용 액터입니다.
 * 셀마다 컴포넌트를 만들지 않고 층마다 셀 상태 텍스처 1장과 데칼 1개로 모든 칸을 그립니다.
 */
UCLASS()
class STRIKEFORGE_API ASFGridDecalVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ASFGridDecalVisualizer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 모든 데칼에 같은 부모 머티리얼의 MID를 만들고 격자 좌표계를 전달합니다. */
	void InitializeVisualizer(UMaterialInterface* InCellDecalMaterial, float InCellSize, const FVector& InGridOrigin);

	/** 막힌 셀은 빨간색, 강조 자원 셀은 초록색으로 표시합니다. */
	void ShowOccupiedCells(
		const TArray<FIntPoint>& BlockedCells,
		const TArray<FIntPoint>& ResourceCells,
		const FFloatInterval& HeightRange);

	/** 이미 지은 시너지 건물들의 클러스터 링크 범위를 하늘색으로 표시합니다. */
	void ShowLinkRangeCells(
		const TArray<FIntPoint>& LinkCells,
		const FFloatInterval& HeightRange);

	/**
	 * 배치 중인 건물 영역 전체를 배치 가능 여부에 따라 초록색 또는 빨간색으로 표시합니다.
	 * WorldCenter는 격자 Subsystem이 GridInfo로 스냅한 월드 중심입니다.
	 */
	void ShowPlacementPreview(const FBuildingGridInfo& GridInfo, const FVector& WorldCenter, bool bCanPlace);

	void HidePlacementPreview();

	/** 배치 모드 표시(점유·링크·미리보기)를 숨깁니다. 시너지 완성 표시는 스스로 사라집니다. */
	void HidePlacementDisplays();

	/** 건물이 속한 활성 시너지 클러스터의 구역 전체를 시너지 완성 표시로 한 번 보여 줍니다. */
	void FlashSynergyClustersOfBuilding(const ASFBaseBuilding* Building);

	/**
	 * 에디터 빌드는 PSO 사전 컴파일이 꺼져 있어 데칼을 처음 그릴 때 끊깁니다.
	 * 로딩 화면 동안 카메라 앞에 투명한 데칼을 그려 PSO를 미리 만듭니다.
	 * 로딩이 끝나면 EndWarmUp으로 숨겨야 합니다.
	 */
	void BeginWarmUp(APlayerController* ViewController);
	void EndWarmUp();

private:
	/** 시너지 구역이 새로 생긴 클러스터의 구역 전체를 노란색으로 표시한 뒤 서서히 지웁니다. */
	UFUNCTION()
	void HandleSynergyStateChanged();

	/** R 채널 칸은 층의 기본 색, B 채널 칸은 자원 색으로 칠하도록 텍스처와 데칼 범위를 맞춥니다. */
	bool ShowMaskLayer(
		FSFGridMaskDecalLayer& Layer,
		const TArray<FIntPoint>& PrimaryCells,
		const TArray<FIntPoint>& SecondaryCells,
		const FFloatInterval& HeightRange);

	/** 셀 상태 텍스처 크기를 맞추고 R = 기본 칸, B = 자원 칸을 기록합니다. */
	void UploadCellStates(
		FSFGridMaskDecalLayer& Layer,
		const FIntPoint& MinCell,
		const FIntPoint& MaskSize,
		const TArray<FIntPoint>& PrimaryCells,
		const TArray<FIntPoint>& SecondaryCells);

	/** 지정한 클러스터들의 구역을 EffectSizeCells 칸으로 펼쳐 노란색으로 표시한 뒤 서서히 지웁니다. */
	void FlashSynergyClusters(
		const TArray<FSFBuildingSynergyZoneInfo>& Zones,
		const TSet<int32>& ClusterIndices);

	void StartSynergyFlash(const TArray<FIntPoint>& Cells, const FFloatInterval& HeightRange);
	void StopSynergyFlash();

	UPROPERTY(VisibleAnywhere, Category = "Grid|Decal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	FSFGridMaskDecalLayer OccupiedLayer;

	UPROPERTY()
	FSFGridMaskDecalLayer LinkRangeLayer;

	UPROPERTY()
	FSFGridMaskDecalLayer SynergyFlashLayer;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Decal")
	TObjectPtr<UDecalComponent> PlacementPreviewDecal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlacementPreviewMID;

	/** 배치 미리보기 데칼의 위아래 투영 깊이입니다. 건물 영역 안의 지형 높낮이 차이보다 커야 합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Grid|Decal")
	float PreviewHalfDepth = 500.f;

	/** 마스크 데칼은 표시 대상 높이 범위에 이 값만큼 여유를 더해 투영합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Grid|Decal")
	float MaskHalfDepthMargin = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grid|Decal")
	FLinearColor LinkRangeColor = FLinearColor(0.2f, 0.75f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Grid|Decal")
	FLinearColor SynergyFlashColor = FLinearColor(1.f, 0.85f, 0.1f, 1.f);

	/** 시너지 완성 표시가 완전히 사라질 때까지 걸리는 시간(초)입니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Grid|Decal", meta = (ClampMin = "0.1"))
	float SynergyFlashDuration = 3.f;

	using FSynergyZoneKey = TTuple<const USFBuildingSynergyDefinition*, FIntPoint, FIntPoint>;

	/** 직전 시너지 재계산의 내 팀 구역입니다. 새로 생긴 구역을 찾는 기준입니다. */
	TSet<FSynergyZoneKey> KnownSynergyZoneKeys;
	bool bHasSynergySnapshot = false;

	float SynergyFlashElapsed = 0.f;

	/** 머티리얼에 설정된 기본 투명도입니다. 예열과 시너지 표시가 끝나면 이 값으로 되돌립니다. */
	float DefaultFillOpacity = 0.6f;
	bool bWarmingUp = false;

	float CellSize = 100.f;
	FVector GridOrigin = FVector::ZeroVector;
};
