#include "Game/Grid/SFGridDecalVisualizer.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Datas/Building/Synergy/SFBuildingSynergyDefinition.h"
#include "Engine/Texture2D.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Logging/SFLog.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 셀 상태 텍스처 한 변의 최대 셀 수입니다. 100u 기준 102400u까지 표시합니다.
	constexpr int32 MaxCellStateTextureSize = 1024;

	// 배치 중 동시에 표시할 수 있는 효과 구역 시너지 수입니다. 현재 최대 2개(특수 병영)에 여유 1개를 둡니다.
	constexpr int32 EffectZoneLayerCount = 3;

	// Pitch -90 데칼은 로컬 X가 투영 깊이, 로컬 Y가 월드 Y, 로컬 Z가 월드 X라 그 순서로 반 크기를 만듭니다.
	// ShowMaskLayer와 ShowPlacementPreview가 데칼 크기를 정할 때 사용합니다.
	FVector MakeDownwardDecalSize(const FVector2D& WorldSize, float HalfDepth)
	{
		return FVector(HalfDepth, WorldSize.Y * 0.5, WorldSize.X * 0.5);
	}
}

// 점유·링크·효과 구역·시너지 완성 마스크 데칼과 배치 미리보기 데칼 1개를 아래로 투영하도록 만듭니다.
// 머티리얼은 UBuildingPlacement::GetOrCreateGridVisualizer가 부르는 InitializeVisualizer에서 지정합니다.
ASFGridDecalVisualizer::ASFGridDecalVisualizer()
{
	// 시너지 완성 표시가 서서히 사라지는 동안에만 Tick 합니다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto CreateGridDecal = [this](const TCHAR* Name, int32 InSortOrder)
	{
		UDecalComponent* Decal = CreateDefaultSubobject<UDecalComponent>(Name);
		Decal->SetupAttachment(Root);
		Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		Decal->FadeScreenSize = 0.f;
		Decal->SortOrder = InSortOrder;
		Decal->SetVisibility(false);
		return Decal;
	};

	OccupiedLayer.Decal = CreateGridDecal(TEXT("OccupiedCellsDecal"), 0);
	LinkRangeLayer.Decal = CreateGridDecal(TEXT("LinkRangeDecal"), 0);

	for (int32 Index = 0; Index < EffectZoneLayerCount; ++Index)
	{
		FSFGridMaskDecalLayer& EffectLayer = EffectZoneLayers.AddDefaulted_GetRef();
		EffectLayer.Decal = CreateGridDecal(
			*FString::Printf(TEXT("EffectZoneDecal%d"), Index), 0);
	}

	SynergyFlashLayer.Decal = CreateGridDecal(TEXT("SynergyFlashDecal"), 1);
	// 점유·링크 표시 위에 그립니다.
	PlacementPreviewDecal = CreateGridDecal(TEXT("PlacementPreviewDecal"), 1);
}

// USFBuildingSynergySubsystem::OnSynergyStateChanged를 HandleSynergyStateChanged에 연결합니다.
// 지금 있는 시너지 구역을 기준으로 저장해 이미 완성된 시너지는 표시하지 않습니다.
void ASFGridDecalVisualizer::BeginPlay()
{
	Super::BeginPlay();

	if (USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this))
	{
		Synergy->OnSynergyStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleSynergyStateChanged);
	}

	// 지금 있는 구역을 기준으로 저장해 이미 완성돼 있던 시너지는 표시하지 않습니다.
	HandleSynergyStateChanged();
}

// BeginPlay에서 연결한 시너지 변경 알림을 해제합니다.
void ASFGridDecalVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this))
	{
		Synergy->OnSynergyStateChanged.RemoveDynamic(
			this, &ThisClass::HandleSynergyStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

// 모든 데칼에 M_SFGridCellDecal의 MID를 만들고 CellSize·GridOrigin·층별 색을 설정합니다.
// UBuildingPlacement::GetOrCreateGridVisualizer가 스폰 직후 한 번 호출합니다.
void ASFGridDecalVisualizer::InitializeVisualizer(
	UMaterialInterface* InCellDecalMaterial,
	float InCellSize,
	const FVector& InGridOrigin)
{
	CellSize = InCellSize;
	GridOrigin = InGridOrigin;

	if (!IsValid(InCellDecalMaterial))
	{
		SF_LOG(Warning, "[GridDecal] 격자 데칼 머티리얼이 없습니다.");
		return;
	}

	// 모든 데칼이 같은 월드 격자에 맞춰 90u 칸을 그립니다.
	const FLinearColor GridOriginParam(
		static_cast<float>(GridOrigin.X),
		static_cast<float>(GridOrigin.Y),
		0.f, 0.f);

	auto CreateGridMID = [&](UDecalComponent* Decal, float UseMask)
	{
		Decal->SetDecalMaterial(InCellDecalMaterial);
		UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance();

		if (IsValid(MID))
		{
			MID->SetScalarParameterValue(TEXT("CellSize"), CellSize);
			MID->SetVectorParameterValue(TEXT("GridOrigin"), GridOriginParam);
			MID->SetScalarParameterValue(TEXT("UseMask"), UseMask);
		}

		return MID;
	};

	OccupiedLayer.MID = CreateGridMID(OccupiedLayer.Decal, 1.f);
	LinkRangeLayer.MID = CreateGridMID(LinkRangeLayer.Decal, 1.f);
	SynergyFlashLayer.MID = CreateGridMID(SynergyFlashLayer.Decal, 1.f);
	PlacementPreviewMID = CreateGridMID(PlacementPreviewDecal, 0.f);

	for (FSFGridMaskDecalLayer& EffectLayer : EffectZoneLayers)
	{
		EffectLayer.MID = CreateGridMID(EffectLayer.Decal, 1.f);
	}

	// 링크 범위는 R 채널 칸을 하늘색으로 칠합니다.
	// 시너지 완성 표시와 효과 구역 색은 시너지마다 달라 StartSynergyFlash·ShowReceivableEffectCells에서 정합니다.
	if (IsValid(LinkRangeLayer.MID))
	{
		LinkRangeLayer.MID->SetVectorParameterValue(TEXT("BlockedColor"), LinkRangeColor);
	}

	if (IsValid(OccupiedLayer.MID))
	{
		DefaultFillOpacity = OccupiedLayer.MID->K2_GetScalarParameterValue(TEXT("FillOpacity"));
	}
}

// 막힌 셀(빨강)과 채굴 가능한 자원 셀(초록)을 점유 층에 그립니다.
// UBuildingPlacement::StartPlacementMode가 USFBuildingGridSubsystem::CollectPlacementDisplayCells 결과로 호출합니다.
void ASFGridDecalVisualizer::ShowOccupiedCells(
	const TArray<FIntPoint>& BlockedCells,
	const TArray<FIntPoint>& ResourceCells,
	const FFloatInterval& HeightRange)
{
	// 예열 중에 실제 표시가 시작되면 예열을 먼저 끝내고 투명도를 되돌립니다.
	EndWarmUp();

	ShowMaskLayer(OccupiedLayer, BlockedCells, ResourceCells, HeightRange);
}

// 이미 지은 시너지 건물들의 클러스터 링크 범위를 하늘색 층에 그립니다.
// UBuildingPlacement::StartPlacementMode가 CollectExistingLinkRangeCells 결과로 호출합니다.
void ASFGridDecalVisualizer::ShowLinkRangeCells(
	const TArray<FIntPoint>& LinkCells,
	const FFloatInterval& HeightRange)
{
	EndWarmUp();

	ShowMaskLayer(LinkRangeLayer, LinkCells, TArray<FIntPoint>(), HeightRange);
}

// 받을 수 있는 시너지 효과 구역을 시너지마다 EffectZoneLayers 한 층에 그 시너지 색(EffectZoneColor)으로 그립니다.
// UBuildingPlacement::ShowPlacementSynergyHints가 CollectReceivableEffectCells 결과로 호출합니다.
void ASFGridDecalVisualizer::ShowReceivableEffectCells(
	const TMap<const USFBuildingSynergyDefinition*, TArray<FIntPoint>>& CellsBySynergy,
	const FFloatInterval& HeightRange)
{
	EndWarmUp();

	int32 LayerIndex = 0;

	for (const TPair<const USFBuildingSynergyDefinition*, TArray<FIntPoint>>& Entry : CellsBySynergy)
	{
		if (!EffectZoneLayers.IsValidIndex(LayerIndex))
		{
			SF_LOG(Warning, "[GridDecal] 효과 구역 층이 %d개뿐이라 나머지 시너지 표시를 생략합니다.",
				EffectZoneLayers.Num());
			break;
		}

		FSFGridMaskDecalLayer& Layer = EffectZoneLayers[LayerIndex];

		if (IsValid(Layer.MID) && IsValid(Entry.Key))
		{
			Layer.MID->SetVectorParameterValue(TEXT("BlockedColor"), Entry.Key->EffectZoneColor);
		}

		// 표시에 실패한 층(칸 없음·범위 초과)은 다음 시너지가 다시 사용합니다.
		if (ShowMaskLayer(Layer, Entry.Value, TArray<FIntPoint>(), HeightRange))
		{
			++LayerIndex;
		}
	}

	// 이번에 쓰지 않은 층은 숨깁니다.
	for (; LayerIndex < EffectZoneLayers.Num(); ++LayerIndex)
	{
		if (IsValid(EffectZoneLayers[LayerIndex].Decal))
		{
			EffectZoneLayers[LayerIndex].Decal->SetVisibility(false);
		}
	}
}

// 배치 중인 건물 영역 전체를 배치 가능 여부(CanPlace)에 따라 초록/빨강으로 그립니다.
// UBuildingPlacement::UpdatePlacementMode가 격자가 바뀐 프레임에만 호출합니다.
void ASFGridDecalVisualizer::ShowPlacementPreview(
	const FBuildingGridInfo& GridInfo,
	const FVector& WorldCenter,
	bool bCanPlace)
{
	// 예열 중에 실제 표시가 시작되면 예열을 먼저 끝내고 투명도를 되돌립니다.
	EndWarmUp();

	if (!IsValid(PlacementPreviewMID) || !GridInfo.IsValid())
	{
		HidePlacementPreview();
		return;
	}

	const FVector2D WorldSize(
		GridInfo.GridSize.X * CellSize,
		GridInfo.GridSize.Y * CellSize);

	PlacementPreviewMID->SetScalarParameterValue(TEXT("CanPlace"), bCanPlace ? 1.f : 0.f);

	PlacementPreviewDecal->SetWorldLocation(WorldCenter);
	PlacementPreviewDecal->DecalSize = MakeDownwardDecalSize(WorldSize, PreviewHalfDepth);
	PlacementPreviewDecal->MarkRenderStateDirty();
	PlacementPreviewDecal->SetVisibility(true);
}

// 배치 미리보기 데칼만 숨깁니다.
// UBuildingPlacement::SetCurrentPlacementInvalid(지형 밖·확정 실패)가 호출합니다.
void ASFGridDecalVisualizer::HidePlacementPreview()
{
	PlacementPreviewDecal->SetVisibility(false);
}

// 점유·링크·효과 구역·미리보기 데칼을 숨기고, 스스로 사라지는 시너지 표시는 그대로 둡니다.
// UBuildingPlacement::EndPlacementSession과 EndWarmUp이 호출합니다.
void ASFGridDecalVisualizer::HidePlacementDisplays()
{
	OccupiedLayer.Decal->SetVisibility(false);
	LinkRangeLayer.Decal->SetVisibility(false);
	PlacementPreviewDecal->SetVisibility(false);

	for (const FSFGridMaskDecalLayer& EffectLayer : EffectZoneLayers)
	{
		if (IsValid(EffectLayer.Decal))
		{
			EffectLayer.Decal->SetVisibility(false);
		}
	}
}

// 셀 목록을 감싸는 범위에 맞춰 마스크 텍스처와 데칼 위치·크기를 정하고 한 층을 표시합니다.
// ShowOccupiedCells·ShowLinkRangeCells·StartSynergyFlash가 공통으로 쓰며 UploadCellStates로 텍스처를 채웁니다.
bool ASFGridDecalVisualizer::ShowMaskLayer(
	FSFGridMaskDecalLayer& Layer,
	const TArray<FIntPoint>& PrimaryCells,
	const TArray<FIntPoint>& SecondaryCells,
	const FFloatInterval& HeightRange)
{
	if (!IsValid(Layer.Decal))
	{
		return false;
	}

	if (!IsValid(Layer.MID) ||
		(PrimaryCells.IsEmpty() && SecondaryCells.IsEmpty()) ||
		!HeightRange.IsValid())
	{
		Layer.Decal->SetVisibility(false);
		return false;
	}

	// 표시할 셀 전체를 감싸는 범위를 구하고 가장자리에 빈 칸 1줄을 둡니다.
	FIntPoint MinCell(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
	FIntPoint MaxCell(TNumericLimits<int32>::Lowest(), TNumericLimits<int32>::Lowest());

	for (const TArray<FIntPoint>* Cells : { &PrimaryCells, &SecondaryCells })
	{
		for (const FIntPoint& Cell : *Cells)
		{
			MinCell = MinCell.ComponentMin(Cell);
			MaxCell = MaxCell.ComponentMax(Cell);
		}
	}

	MinCell -= FIntPoint(1, 1);
	MaxCell += FIntPoint(1, 1);

	const FIntPoint MaskSize = MaxCell - MinCell + FIntPoint(1, 1);

	if (MaskSize.X > MaxCellStateTextureSize ||
		MaskSize.Y > MaxCellStateTextureSize)
	{
		SF_LOG(Warning, "[GridDecal] 표시 범위가 너무 큽니다. %d x %d", MaskSize.X, MaskSize.Y);
		Layer.Decal->SetVisibility(false);
		return false;
	}

	UploadCellStates(Layer, MinCell, MaskSize, PrimaryCells, SecondaryCells);

	Layer.MID->SetTextureParameterValue(TEXT("CellStateMask"), Layer.Texture);
	Layer.MID->SetVectorParameterValue(TEXT("MaskOriginCell"),
		FLinearColor(static_cast<float>(MinCell.X), static_cast<float>(MinCell.Y), 0.f, 0.f));
	Layer.MID->SetVectorParameterValue(TEXT("MaskSize"),
		FLinearColor(static_cast<float>(MaskSize.X), static_cast<float>(MaskSize.Y), 0.f, 0.f));

	const FVector2D WorldMin(
		GridOrigin.X + MinCell.X * CellSize,
		GridOrigin.Y + MinCell.Y * CellSize);
	const FVector2D WorldSize(
		MaskSize.X * CellSize,
		MaskSize.Y * CellSize);

	Layer.Decal->SetWorldLocation(FVector(
		WorldMin.X + WorldSize.X * 0.5,
		WorldMin.Y + WorldSize.Y * 0.5,
		(HeightRange.Min + HeightRange.Max) * 0.5f));
	Layer.Decal->DecalSize = MakeDownwardDecalSize(
		WorldSize,
		HeightRange.Size() * 0.5f + MaskHalfDepthMargin);
	Layer.Decal->MarkRenderStateDirty();
	Layer.Decal->SetVisibility(true);

	return true;
}

// 층의 CPU 텍스처(텍셀 1개 = 셀 1개)를 만들거나 재사용해 R·B 채널에 셀 상태를 기록합니다.
// ShowMaskLayer가 호출하며 USFFogOfWarSubsystem과 같은 UpdateTextureRegions 방식입니다.
void ASFGridDecalVisualizer::UploadCellStates(
	FSFGridMaskDecalLayer& Layer,
	const FIntPoint& MinCell,
	const FIntPoint& MaskSize,
	const TArray<FIntPoint>& PrimaryCells,
	const TArray<FIntPoint>& SecondaryCells)
{
	if (!IsValid(Layer.Texture) ||
		Layer.Texture->GetSizeX() != MaskSize.X ||
		Layer.Texture->GetSizeY() != MaskSize.Y)
	{
		// 전장의 안개와 같은 방식의 CPU 텍스처입니다. 텍셀 1개가 셀 1개입니다.
		Layer.Texture = UTexture2D::CreateTransient(MaskSize.X, MaskSize.Y, PF_B8G8R8A8);
		Layer.Texture->SRGB = false;
		Layer.Texture->NeverStream = true;
		Layer.Texture->Filter = TF_Nearest;
		Layer.Texture->AddressX = TA_Clamp;
		Layer.Texture->AddressY = TA_Clamp;
		Layer.Texture->UpdateResource();
	}

	const int32 PixelCount = MaskSize.X * MaskSize.Y;
	FColor* Pixels = new FColor[PixelCount];

	for (int32 Index = 0; Index < PixelCount; ++Index)
	{
		Pixels[Index] = FColor(0, 0, 0, 0);
	}

	auto ToPixelIndex = [&MinCell, &MaskSize](const FIntPoint& Cell)
	{
		const FIntPoint Local = Cell - MinCell;
		return Local.Y * MaskSize.X + Local.X;
	};

	// R = 기본 칸(층의 BlockedColor), B = 강조 자원 칸(ResourceColor). G는 사용하지 않습니다.
	for (const FIntPoint& Cell : PrimaryCells)
	{
		Pixels[ToPixelIndex(Cell)].R = 255;
	}

	for (const FIntPoint& Cell : SecondaryCells)
	{
		Pixels[ToPixelIndex(Cell)].B = 255;
	}

	FUpdateTextureRegion2D* Region =
		new FUpdateTextureRegion2D(0, 0, 0, 0, MaskSize.X, MaskSize.Y);

	Layer.Texture->UpdateTextureRegions(
		0, 1, Region,
		MaskSize.X * sizeof(FColor), sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels),
		[](uint8* Data, const FUpdateTextureRegion2D* UpdateRegion)
		{
			delete[] reinterpret_cast<FColor*>(Data);
			delete UpdateRegion;
		});
}

// 내 팀 시너지 구역을 직전 결과와 비교해 새 구역이 생긴 클러스터를 시너지 색으로 표시합니다.
// USFBuildingSynergySubsystem::OnSynergyStateChanged에 연결되며 FlashSynergyClusters를 호출합니다.
void ASFGridDecalVisualizer::HandleSynergyStateChanged()
{
	const USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this);
	const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);

	if (!IsValid(Synergy) || !IsValid(Combat))
	{
		return;
	}

	const TArray<FSFBuildingSynergyZoneInfo> Zones =
		Synergy->GetSynergyZonesForTeam(Combat->GetLocalPlayerTeam());

	// 직전 결과에 없던 구역이 생긴 클러스터를 찾습니다.
	TSet<FSynergyZoneKey> CurrentZoneKeys;
	TSet<int32> FlashClusterIndices;

	for (const FSFBuildingSynergyZoneInfo& Zone : Zones)
	{
		const FSynergyZoneKey ZoneKey(
			Zone.Definition.Get(),
			Zone.EffectGridInfo.OriginCell,
			Zone.EffectGridInfo.GridSize);

		CurrentZoneKeys.Add(ZoneKey);

		if (!KnownSynergyZoneKeys.Contains(ZoneKey))
		{
			FlashClusterIndices.Add(Zone.ClusterIndex);
		}
	}

	KnownSynergyZoneKeys = MoveTemp(CurrentZoneKeys);

	const bool bHadSnapshot = bHasSynergySnapshot;
	bHasSynergySnapshot = true;

	// 첫 조회와 스테이지 로딩 중에 생긴 구역(미리 배치된 건물)은 기준으로만 저장합니다.
	const UWorld* World = GetWorld();
	const ASFGameState* SFGameState =
		IsValid(World) ? World->GetGameState<ASFGameState>() : nullptr;

	if (!bHadSnapshot ||
		FlashClusterIndices.IsEmpty() ||
		!IsValid(SFGameState) ||
		!SFGameState->IsStagePlaying())
	{
		return;
	}

	FlashSynergyClusters(Zones, FlashClusterIndices);
}

// 선택한 내 건물이 원천인 구역으로 활성 클러스터를 찾아 시너지 색으로 한 번 표시합니다.
// ASFInGamePlayerController::SetSelectedActor → UBuildingPlacement::FlashSynergyClustersOfBuilding에서 호출됩니다.
void ASFGridDecalVisualizer::FlashSynergyClustersOfBuilding(const ASFBaseBuilding* Building)
{
	const USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this);
	const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);

	if (!IsValid(Building) || !IsValid(Synergy) || !IsValid(Combat))
	{
		return;
	}

	// 내 팀 건물만 표시합니다. 적 건물을 선택해도 적의 시너지 구역은 보여 주지 않습니다.
	const int32 LocalTeam = Combat->GetLocalPlayerTeam();
	if (Building->GetTeamIndex() != LocalTeam)
	{
		return;
	}

	const TArray<FSFBuildingSynergyZoneInfo> Zones =
		Synergy->GetSynergyZonesForTeam(LocalTeam);

	// 이 건물을 원천으로 하는 구역이 있어야 활성 클러스터에 속한 것입니다.
	TSet<int32> ClusterIndices;

	for (const FSFBuildingSynergyZoneInfo& Zone : Zones)
	{
		if (Zone.SourceBuilding.Get() == Building)
		{
			ClusterIndices.Add(Zone.ClusterIndex);
		}
	}

	if (!ClusterIndices.IsEmpty())
	{
		FlashSynergyClusters(Zones, ClusterIndices);
	}
}

// 지정한 클러스터 구역들을 EffectSizeCells 칸으로 펼쳐 StartSynergyFlash에 넘깁니다.
// HandleSynergyStateChanged(시너지 완성)와 FlashSynergyClustersOfBuilding(건물 선택)이 공통으로 사용합니다.
void ASFGridDecalVisualizer::FlashSynergyClusters(
	const TArray<FSFBuildingSynergyZoneInfo>& Zones,
	const TSet<int32>& ClusterIndices)
{
	// 지정한 클러스터의 모든 구역을 EffectSizeCells 칸으로 펼칩니다.
	TSet<FIntPoint> FlashCells;
	FFloatInterval HeightRange;

	// 색은 시너지 Definition의 EffectZoneColor를 씁니다(주거 = 노랑, 군사 = 초록).
	// 한 번에 여러 시너지가 표시되면 처음 찾은 시너지의 색을 사용합니다.
	const USFBuildingSynergyDefinition* ColorDefinition = nullptr;

	for (const FSFBuildingSynergyZoneInfo& Zone : Zones)
	{
		if (!ClusterIndices.Contains(Zone.ClusterIndex))
		{
			continue;
		}

		if (ColorDefinition == nullptr && IsValid(Zone.Definition))
		{
			ColorDefinition = Zone.Definition;
		}

		const FBuildingGridInfo& EffectGridInfo = Zone.EffectGridInfo;

		for (int32 Y = 0; Y < EffectGridInfo.GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < EffectGridInfo.GridSize.X; ++X)
			{
				FlashCells.Add(EffectGridInfo.OriginCell + FIntPoint(X, Y));
			}
		}

		HeightRange.Include(Zone.WorldCenter.Z);
	}

	const FLinearColor FlashColor = IsValid(ColorDefinition)
		? ColorDefinition->EffectZoneColor
		: GetDefault<USFBuildingSynergyDefinition>()->EffectZoneColor;

	StartSynergyFlash(FlashCells.Array(), HeightRange, FlashColor);
}

// 시너지 층을 지정한 색과 기본 투명도로 ShowMaskLayer에 표시하고 Tick을 켜 사라지는 효과를 시작합니다.
// FlashSynergyClusters가 시너지 Definition의 EffectZoneColor로 호출합니다.
void ASFGridDecalVisualizer::StartSynergyFlash(
	const TArray<FIntPoint>& Cells,
	const FFloatInterval& HeightRange,
	const FLinearColor& FlashColor)
{
	if (IsValid(SynergyFlashLayer.MID))
	{
		SynergyFlashLayer.MID->SetVectorParameterValue(TEXT("BlockedColor"), FlashColor);
	}

	if (!ShowMaskLayer(SynergyFlashLayer, Cells, TArray<FIntPoint>(), HeightRange))
	{
		StopSynergyFlash();
		return;
	}

	SynergyFlashElapsed = 0.f;
	SynergyFlashLayer.MID->SetScalarParameterValue(TEXT("FillOpacity"), DefaultFillOpacity);
	SetActorTickEnabled(true);
}

// 시너지 층을 숨기고 Tick을 끈 뒤 투명도를 기본값으로 되돌립니다.
// Tick에서 투명도가 0이 되거나 StartSynergyFlash가 표시에 실패하면 호출됩니다.
void ASFGridDecalVisualizer::StopSynergyFlash()
{
	SetActorTickEnabled(false);
	SynergyFlashLayer.Decal->SetVisibility(false);

	if (IsValid(SynergyFlashLayer.MID))
	{
		SynergyFlashLayer.MID->SetScalarParameterValue(TEXT("FillOpacity"), DefaultFillOpacity);
	}
}

// 시너지 완성 표시가 켜진 동안만 돌며 SynergyFlashDuration에 걸쳐 FillOpacity를 0까지 줄입니다.
// StartSynergyFlash가 켜고 StopSynergyFlash가 끕니다.
void ASFGridDecalVisualizer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 투명도를 기본값에서 0까지 줄이고, 0이 되면 표시를 없앱니다.
	SynergyFlashElapsed += DeltaSeconds;

	const float Remaining = 1.f - SynergyFlashElapsed / SynergyFlashDuration;

	if (Remaining <= 0.f || !IsValid(SynergyFlashLayer.MID))
	{
		StopSynergyFlash();
		return;
	}

	SynergyFlashLayer.MID->SetScalarParameterValue(
		TEXT("FillOpacity"), DefaultFillOpacity * Remaining);
}

// 로딩 중 카메라 앞에 투명 데칼을 그려, 에디터 빌드에서 첫 표시 때 생기는 PSO 생성 끊김을 없앱니다.
// UBuildingPlacement::BeginPlay가 스테이지 로딩 중일 때 호출합니다.
void ASFGridDecalVisualizer::BeginWarmUp(APlayerController* ViewController)
{
	if (bWarmingUp ||
		!IsValid(ViewController) ||
		!IsValid(ViewController->PlayerCameraManager) ||
		!IsValid(OccupiedLayer.MID) ||
		!IsValid(PlacementPreviewMID))
	{
		return;
	}

	bWarmingUp = true;

	// 투명도 0으로 그리므로 화면에는 아무것도 보이지 않습니다.
	// 모든 층이 같은 머티리얼이라 두 데칼만 그려도 같은 PSO가 만들어집니다.
	OccupiedLayer.MID->SetScalarParameterValue(TEXT("FillOpacity"), 0.f);
	PlacementPreviewMID->SetScalarParameterValue(TEXT("FillOpacity"), 0.f);

	// 큰 데칼은 스텐실 사용 경로, 작은 데칼은 스텐실 미사용 경로를 예열합니다.
	OccupiedLayer.Decal->SetRelativeLocation(FVector::ZeroVector);
	PlacementPreviewDecal->SetRelativeLocation(FVector::ZeroVector);
	OccupiedLayer.Decal->DecalSize = FVector(1000.f);
	PlacementPreviewDecal->DecalSize = FVector(100.f);
	OccupiedLayer.Decal->MarkRenderStateDirty();
	PlacementPreviewDecal->MarkRenderStateDirty();
	OccupiedLayer.Decal->SetVisibility(true);
	PlacementPreviewDecal->SetVisibility(true);

	// 카메라 매니저는 매 프레임 실제 카메라 위치로 이동하므로 붙여 두면 카메라 3000u 앞을 따라갑니다.
	// 카메라가 데칼 상자 밖에 있어야 실제 배치 때와 같은 PSO가 만들어집니다.
	AttachToActor(
		ViewController->PlayerCameraManager,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(3000.f, 0.f, 0.f));
}

// 예열을 끝내고 카메라에서 떼어 원점으로 되돌린 뒤 데칼을 숨기고 투명도를 복원합니다.
// UBuildingPlacement::StopGridDecalWarmUp(로딩 종료)과 각 Show 함수가 호출합니다.
void ASFGridDecalVisualizer::EndWarmUp()
{
	if (!bWarmingUp)
	{
		return;
	}

	bWarmingUp = false;

	// 실제 표시는 데칼을 월드 좌표로 배치하므로 원점·무회전 상태로 되돌립니다.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorTransform(FTransform::Identity);

	HidePlacementDisplays();

	if (IsValid(OccupiedLayer.MID))
	{
		OccupiedLayer.MID->SetScalarParameterValue(TEXT("FillOpacity"), DefaultFillOpacity);
	}

	if (IsValid(PlacementPreviewMID))
	{
		PlacementPreviewMID->SetScalarParameterValue(TEXT("FillOpacity"), DefaultFillOpacity);
	}
}
