#include "Buildings/GC/SFBuildingDestructionActor.h"

#include "TimerManager.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "Field/FieldSystemObjects.h"
#include "GeometryCollectionProxyData.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"

namespace SFVFXParameters
{
    const FName ClusterLevel(TEXT("User.ClusterLevel"));
}

struct FSFPieceState
{
    const TArray<FTransform3f>* Transforms = nullptr;
    TOptional<FGeometryCollectionDynamicStateFacade> State;
    int32 NumTransforms = 0;

    bool IsValidPiece(int32 PieceIndex) const
    {
        return Transforms && Transforms->IsValidIndex(PieceIndex);
    }

    float GetScale(int32 PieceIndex) const
    {
        return (*Transforms)[PieceIndex].GetScale3D().GetAbsMax();
    }
    
};

static float RayBoxDistance2D(const FVector& Extent, const FVector& Dir)
{
    float Distance = TNumericLimits<float>::Max();

    if (FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER)
    {
        Distance = FMath::Min(Distance, Extent.X / FMath::Abs(Dir.X));
    }

    if (FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER)
    {
        Distance = FMath::Min(Distance, Extent.Y / FMath::Abs(Dir.Y));
    }

    return Distance;
}

static int32 CalculateRestHierarchyLevel(const TManagedArray<int32>& Parents, int32 TransformIndex)
{
    if (!Parents.IsValidIndex(TransformIndex))
    {
        return INDEX_NONE;
    }

    int32 CurrentIndex = TransformIndex;
    int32 Level = 0;

    for (int32 Guard = 0; Guard < Parents.Num(); ++Guard)
    {
        const int32 ParentIndex = Parents[CurrentIndex];

        if (ParentIndex == INDEX_NONE)
        {
            return Level;
        }

        if (!Parents.IsValidIndex(ParentIndex))
        {
            return INDEX_NONE;
        }

        CurrentIndex = ParentIndex;
        ++Level;
    }

    // 비정상적인 순환 계층
    return INDEX_NONE;
}

static int32 FindAncestorAtRestLevel(
    const TManagedArray<int32>& Parents,
    const TArray<int32>& HierarchyLevels,
    int32 TransformIndex,
    int32 TargetLevel)
{
    if (!HierarchyLevels.IsValidIndex(TransformIndex) ||
        HierarchyLevels[TransformIndex] == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    int32 CurrentIndex = TransformIndex;

    for (int32 Guard = 0; Guard < Parents.Num(); ++Guard)
    {
        if (HierarchyLevels[CurrentIndex] <= TargetLevel)
        {
            return CurrentIndex;
        }

        const int32 ParentIndex = Parents[CurrentIndex];

        if (!Parents.IsValidIndex(ParentIndex))
        {
            return CurrentIndex;
        }

        CurrentIndex = ParentIndex;
    }

    return INDEX_NONE;
}

static float GetBoxSurfaceGapRatio(const FBox& OuterBox, const FBox& InnerBox)
{
    const FVector OuterSize = OuterBox.GetSize();
    float MinGapRatio = 1.f;

    const auto ConsumeAxis =
        [&MinGapRatio](
            float DistanceToMin,
            float DistanceToMax,
            float AxisSize)
        {
            if (AxisSize <= KINDA_SMALL_NUMBER)
            {
                MinGapRatio = 0.f;
                return;
            }

            const float Gap = FMath::Max(
                FMath::Min(DistanceToMin, DistanceToMax),
                0.f);

            MinGapRatio = FMath::Min(
                MinGapRatio,
                Gap / AxisSize);
        };

    ConsumeAxis(
        InnerBox.Min.X - OuterBox.Min.X,
        OuterBox.Max.X - InnerBox.Max.X,
        OuterSize.X);

    ConsumeAxis(
        InnerBox.Min.Y - OuterBox.Min.Y,
        OuterBox.Max.Y - InnerBox.Max.Y,
        OuterSize.Y);

    ConsumeAxis(
        InnerBox.Min.Z - OuterBox.Min.Z,
        OuterBox.Max.Z - InnerBox.Max.Z,
        OuterSize.Z);

    return FMath::Clamp(MinGapRatio, 0.f, 1.f);
}

/**
 * 현재 GC 클러스터에서 바깥과 가장 가까운 조각 면을 찾는다.
 * 클러스터가 조각 하나뿐이라 모든 면의 간격이 같으면 건물 전체 바운드에
 * 가까운 면을 선택한다. 아래쪽(-Z)은 지면에 묻히므로 후보에서 제외한다.
 */
static FVector GetSurfaceFirePoint(
    const FBox& ClusterBounds,
    const FBox& BuildingBounds,
    const FBox& PieceBounds,
    float SurfaceRatio)
{
    const FVector PieceCenter = PieceBounds.GetCenter();
    FVector BestSurfacePoint = PieceCenter;
    float BestClusterGap = TNumericLimits<float>::Max();
    float BestBuildingGap = TNumericLimits<float>::Max();

    const auto TestSurface =
        [&BestSurfacePoint, &BestClusterGap, &BestBuildingGap](
            float ClusterGap,
            float BuildingGap,
            const FVector& SurfacePoint)
        {
            ClusterGap = FMath::Max(ClusterGap, 0.f);
            BuildingGap = FMath::Max(BuildingGap, 0.f);

            const bool bCloserToClusterSurface =
                ClusterGap < BestClusterGap - KINDA_SMALL_NUMBER;

            const bool bSameClusterGapButCloserToBuildingSurface =
                FMath::IsNearlyEqual(ClusterGap, BestClusterGap) &&
                BuildingGap < BestBuildingGap;

            if (bCloserToClusterSurface ||
                bSameClusterGapButCloserToBuildingSurface)
            {
                BestClusterGap = ClusterGap;
                BestBuildingGap = BuildingGap;
                BestSurfacePoint = SurfacePoint;
            }
        };

    TestSurface(
        ClusterBounds.Max.X - PieceBounds.Max.X,
        BuildingBounds.Max.X - PieceBounds.Max.X,
        FVector(PieceBounds.Max.X, PieceCenter.Y, PieceCenter.Z));

    TestSurface(
        PieceBounds.Min.X - ClusterBounds.Min.X,
        PieceBounds.Min.X - BuildingBounds.Min.X,
        FVector(PieceBounds.Min.X, PieceCenter.Y, PieceCenter.Z));

    TestSurface(
        ClusterBounds.Max.Y - PieceBounds.Max.Y,
        BuildingBounds.Max.Y - PieceBounds.Max.Y,
        FVector(PieceCenter.X, PieceBounds.Max.Y, PieceCenter.Z));

    TestSurface(
        PieceBounds.Min.Y - ClusterBounds.Min.Y,
        PieceBounds.Min.Y - BuildingBounds.Min.Y,
        FVector(PieceCenter.X, PieceBounds.Min.Y, PieceCenter.Z));

    TestSurface(
        ClusterBounds.Max.Z - PieceBounds.Max.Z,
        BuildingBounds.Max.Z - PieceBounds.Max.Z,
        FVector(PieceCenter.X, PieceCenter.Y, PieceBounds.Max.Z));

    return FMath::Lerp(
        PieceCenter,
        BestSurfacePoint,
        FMath::Clamp(SurfaceRatio, 0.f, 1.f));
}

static void BuildPieceState(UGeometryCollectionComponent& Component, FSFPieceState& Out)
{
    Out.Transforms = &Component.GetComponentSpaceTransforms3f();

    FGeometryDynamicCollection* Dynamic = Component.GetDynamicCollection();

    Out.NumTransforms = Dynamic ? Dynamic->GetNumTransforms() : 0;

    // 루트가 멀쩡하면 모든 조각이 본체 소속이다. 파사드 자체가 필요 없다.
    if (Dynamic && Component.IsRootBroken())
    {
        Out.State.Emplace(*Dynamic);

        if (!Out.State->IsValid())
        {
            Out.State.Reset();
        }
    }
}

ASFBuildingDestructionActor::ASFBuildingDestructionActor()
{
    PrimaryActorTick.bCanEverTick = false;

    GeometryCollectionComponent =
        CreateDefaultSubobject<UGeometryCollectionComponent>(
            TEXT("GeometryCollectionComponent"));

    SetRootComponent(GeometryCollectionComponent);

    GeometryCollectionComponent->SetMobility(EComponentMobility::Movable);
    GeometryCollectionComponent->SetVisibility(false);
    // 완성 메시를 대신 표시하므로 배치 격자 데칼이 묻지 않도록 합니다.
    GeometryCollectionComponent->bReceivesDecals = false;
    GeometryCollectionComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    GeometryCollectionComponent->SetSimulatePhysics(false);
    GeometryCollectionComponent->SetEnableGravity(false);
    GeometryCollectionComponent->SetCanEverAffectNavigation(false);

    GeometryCollectionComponent->ObjectType =
        EObjectStateTypeEnum::Chaos_Object_Dynamic;

    GeometryCollectionComponent->bIgnoreRadialImpulse = false;
    GeometryCollectionComponent->bIgnoreRadialForce = false;
    GeometryCollectionComponent->BodyInstance.bStartAwake = true;

    StrainField =
        CreateDefaultSubobject<URadialFalloff>(
            TEXT("StrainField"));
}

bool ASFBuildingDestructionActor::InitializeDestruction(const UGeometryCollection* InCollection)
{

    if (bInitialized)
    {
        return true;
    }

    if (!IsValid(GeometryCollectionComponent) ||
        !IsValid(StrainField) ||
        !IsValid(InCollection))
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Building destruction initialization failed."),
            *GetName());

        return false;
    }
    
    GeometryCollectionComponent->SetRestCollection(InCollection,true);
    // 최종 파괴 시 활성 클러스터를 계속 해체하기 위해 유지
    GeometryCollectionComponent->SetNotifyBreaks(true);
    GeometryCollectionComponent->OnChaosBreakEvent.AddUniqueDynamic(
        this, &ThisClass::HandleChaosBreak);
    
    // 파괴 radius의 위치와 크기
    const FBox LocalBounds = GeometryCollectionComponent->GetLocalBounds();

    if (LocalBounds.IsValid)
    {
        // 파괴 전 원본 바운드. Strain 필드 위치·반경과 불꽃 배치 기준을 모두 여기서 유도한다.
        CachedRestBounds = LocalBounds;
    }
    
    // 단계별 파괴 실험 중에는 Damage 전파 차단
    FGeometryCollectionDamagePropagationData NoPropagation;
    NoPropagation.bEnabled = false;
    NoPropagation.BreakDamagePropagationFactor = 0.f;
    NoPropagation.ShockDamagePropagationFactor = 0.f;

    GeometryCollectionComponent->SetDamagePropagationData(NoPropagation);

    // 떨어지거나 서로 충돌하면서 다음 단계가 깨지는 것 방지
    GeometryCollectionComponent->SetEnableDamageFromCollision(false);
    GeometryCollectionComponent->SetCollisionResponseToChannel(
        ECC_Pawn,ECR_Ignore);
    
    // 마우스 선택도 잔해를 통과시킨다.
    GeometryCollectionComponent->SetCollisionResponseToChannel(
        ECC_Visibility,ECR_Ignore);
    
    GeometryCollectionComponent->SetGenerateOverlapEvents(false);
    GeometryCollectionComponent->SetCanEverAffectNavigation(false);
    
    /*
     * 건설 완료 즉시 GC를 활성화한다.
     * Root가 깨지기 전에는 Root Proxy가 완성 메쉬를 표시한다.
     */
    GeometryCollectionComponent->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);

    GeometryCollectionComponent->SetEnableGravity(true);
    GeometryCollectionComponent->SetSimulatePhysics(true);
    GeometryCollectionComponent->SetVisibility(true, true);

    // 충돌로는 깨지지 않고, 코드에서 적용한 Strain으로만 파괴된다.
    GeometryCollectionComponent->bIgnoreRadialImpulse = false;
    GeometryCollectionComponent->bIgnoreRadialForce = false;

    bInitialized = true;
    return true;
}

void ASFBuildingDestructionActor::SetSurfaceFireEffect(UNiagaraSystem* InEffect)
{
    SurfaceFireEffect = InEffect;
}

void ASFBuildingDestructionActor::ApplyDamageStage(int32 StageIndex, const FBuildingDestruction& StageData)
{
    if (!bInitialized || bFinalDestructionApplied ||
        StageIndex < 0 || StageIndex <= AppliedStageIndex)
    {
        return;
    }

    AppliedStageIndex = StageIndex;

    ApplyStageSettings(StageData, StageIndex + 1);
}

void ASFBuildingDestructionActor::SetSurfaceFireStep(int32 StepIndex, int32 FireCount)
{
    if (!bInitialized || bFinalDestructionApplied || StepIndex <= SurfaceFireStep)
    {
        return;
    }

    SurfaceFireStep = StepIndex;
    SurfaceFireTarget = FMath::Max(FireCount, 0);

    // 잠들어 있으면 깨워서 부족분을 채우게 한다.
    StartSurfaceFireMaintenance();
}

void ASFBuildingDestructionActor::ApplyFinalDestruction(const int32 FinalStageIndex, const FBuildingDestruction& StageData)
{
    if (!bInitialized ||
         bFinalDestructionApplied ||
         FinalStageIndex < 0)
    {
        return;
    }

    bFinalDestructionApplied = true;

    GeometryCollectionComponent->RemoveAllAnchors();

    ApplyStageSettings(StageData, FinalStageIndex + 1);
    SetLifeSpan(DebrisLifetime);
}

void ASFBuildingDestructionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);

    GetWorldTimerManager().ClearTimer(StrainHitTimerHandle);

    GetWorldTimerManager().ClearTimer(ImpulseTimerHandle);

    if (IsValid(GeometryCollectionComponent))
    {
        GeometryCollectionComponent->OnChaosBreakEvent.RemoveDynamic(
            this,
            &ThisClass::HandleChaosBreak);
    }

    SurfaceFires.Reset();

    Super::EndPlay(EndPlayReason);
}

void ASFBuildingDestructionActor::ApplyStageSettings(const FBuildingDestruction& StageData, const int32 TargetClusterDepth)
{
    if (!IsValid(GeometryCollectionComponent) ||
        !IsValid(StrainField))
    {
        return;
    }

    const FTransform& ComponentTransform =
        GeometryCollectionComponent->GetComponentTransform();

    const FVector LocalFieldCenter =
        CachedRestBounds.GetCenter() + StageData.LocalOffset;

    const FVector WorldCenter =
        ComponentTransform.TransformPosition(LocalFieldCenter);
    
    const float ComponentScale = GetComponentUniformScale();

    const float WorldRadius =
        FMath::Max(CachedRestBounds.GetExtent().Size(), 1.f) *
            StageData.RadiusScale * ComponentScale;

    /**
     * Falloff None:
     * 반경 안의 파편에는 지정한 Strain이 그대로 적용된다.
     * 초기 단계 검증 시 가장 예측하기 쉽다.
     */
    StrainField->SetRadialFalloff(
        StageData.Strain,
        0.f,
        1.f,
        0.f,
        WorldRadius,
        WorldCenter,
        EFieldFalloffType::Field_FallOff_None);
    
    // 이전 단계의 예약 작업이 남아 있다면 새 목표로 교체한다.
    GetWorldTimerManager().ClearTimer(StrainHitTimerHandle);

    GetWorldTimerManager().ClearTimer(ImpulseTimerHandle);

    PendingTargetClusterDepth = FMath::Max(TargetClusterDepth, 1);

    PendingImpulseOrigin = WorldCenter;
    PendingImpulseRadius = WorldRadius;
    PendingImpulseStrength = StageData.ImpulseStrength;
    PendingImpulseDelay = StageData.ImpulseDelay;

    ApplyNextStrainHit();
}

void ASFBuildingDestructionActor::ApplyNextStrainHit()
{
    if (!bInitialized ||
        !IsValid(GeometryCollectionComponent) ||
        !IsValid(StrainField))
    {
        return;
    }

    if (AppliedClusterDepth >= PendingTargetClusterDepth)
    {
        SchedulePendingImpulse();
        return;
    }

    GeometryCollectionComponent->WakeAllRigidBodies();

    GeometryCollectionComponent->ApplyPhysicsField(
        true,
        EGeometryCollectionPhysicsTypeEnum::
            Chaos_ExternalClusterStrain,
        nullptr,
        StrainField);

    ++AppliedClusterDepth;

    UE_LOG(LogTemp, Log, TEXT("[%s] Strain hit: %d / %d"),
        *GetName(), AppliedClusterDepth, PendingTargetClusterDepth);

    if (AppliedClusterDepth < PendingTargetClusterDepth)
    {
        // 같은 타이머 핸들을 다음 평가에 재사용한다.
        GetWorldTimerManager().SetTimer(
            StrainHitTimerHandle,
            this,
            &ThisClass::ApplyNextStrainHit,
            StrainHitInterval,
            false);

        return;
    }

    SchedulePendingImpulse();
}

void ASFBuildingDestructionActor::SchedulePendingImpulse()
{
    GetWorldTimerManager().ClearTimer(ImpulseTimerHandle);
    
    if (bFinalDestructionApplied)
    {
        GeometryCollectionComponent->CrumbleActiveClusters();
    }
    
    // 최종 파괴 후에도 남은 불을 정리하려면 루프가 계속 돌아야 한다.
    StartSurfaceFireMaintenance();
    
    if (PendingImpulseStrength <= 0.f)
    {
        return;
    }
    
    if (PendingImpulseDelay <= 0.f)
    {
        ApplyPendingImpulse();
        return;
    }
    
    GetWorldTimerManager().SetTimer(
        ImpulseTimerHandle,
        this,
        &ThisClass::ApplyPendingImpulse,
        PendingImpulseDelay,
        false);
}

void ASFBuildingDestructionActor::ApplyPendingImpulse()
{
    if (!bInitialized ||
        !IsValid(GeometryCollectionComponent) ||
        PendingImpulseStrength <= 0.f)
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[%s] Radial impulse fired: Strength=%.1f Radius=%.1f Origin=%s"),
        *GetName(), PendingImpulseStrength, PendingImpulseRadius, *PendingImpulseOrigin.ToString());
    
    GeometryCollectionComponent->AddRadialImpulse(
        PendingImpulseOrigin,
        PendingImpulseRadius,
        PendingImpulseStrength,
        ERadialImpulseFalloff::RIF_Linear,
        true); // 질량과 관계없이 속도 변화 적용

    PendingImpulseStrength = 0.f;
}

void ASFBuildingDestructionActor::HandleChaosBreak(const FChaosBreakEvent& BreakEvent)
{
    if (!bInitialized ||
        BreakEvent.Component != GeometryCollectionComponent)
    {
        return;
    }

    /*
     * 같은 프레임에 여러 Break 이벤트가 발생해도
     * Crumble과 불꽃 갱신은 한 번만 수행한다.
     */
    if (LastBreakWakeFrame == GFrameCounter)
    {
        return;
    }

    LastBreakWakeFrame = GFrameCounter;

    if (bFinalDestructionApplied)
    {
        // 새롭게 활성화된 하위 클러스터까지 해체한다.
        GeometryCollectionComponent->CrumbleActiveClusters();
    }

    if (!GetWorldTimerManager().IsTimerActive(
            SurfaceFireTimerHandle))
    {
        StartSurfaceFireMaintenance();
    }
}

void ASFBuildingDestructionActor::StartSurfaceFireMaintenance()
{
    if (!IsValid(GeometryCollectionComponent))
    {
        return;
    }

    /*
     * 최종 파괴 후에는 새 불을 보충하지 않는다.
     * 남은 불의 축소를 감지해 정리하기 위해서만 루프를 돌린다.
     */
    if (bFinalDestructionApplied)
    {
        if (SurfaceFires.IsEmpty())
        {
            return;
        }
    }
    else if (!IsValid(SurfaceFireEffect))
    {
        return;
    }
    
    if (!GetWorldTimerManager().IsTimerActive(SurfaceFireTimerHandle))
    {
        GetWorldTimerManager().SetTimer(
            SurfaceFireTimerHandle,
            this,
            &ThisClass::UpdateSurfaceFires,
            SurfaceFireUpdateInterval,
            true);
    }

    UpdateSurfaceFires();
}

void ASFBuildingDestructionActor::UpdateSurfaceFires()
{
    if (!IsValid(GeometryCollectionComponent))
    {
        GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);
        SurfaceFires.Reset();
        return;
    }
    
    FSFPieceState Pieces;
    BuildPieceState(*GeometryCollectionComponent, Pieces);

    int32 AttachedFireCount = 0;
    bool bAnyDetached = false;

    for (int32 Index = SurfaceFires.Num() - 1; Index >= 0; --Index)
    {
        FSFSurfaceFire& Fire = SurfaceFires[Index];

        if (!IsValid(Fire.Component))
        {
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }
        
        if (!Pieces.IsValidPiece(Fire.PieceIndex))
        {
            RemoveSurfaceFire(Fire);
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }

        /*
         * 계층 분리 판정보다 먼저 검사한다.
         * 빠른 파괴로 분리 상태를 놓쳐도 조각 축소는 감지할 수 있다.
         */
        if (IsPieceShrunk(Pieces, Fire.PieceIndex, Fire.RestScale))
        {
            RemoveSurfaceFire(Fire);
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }

        if (!Fire.bDetached)
        {
            Fire.bDetached = !IsPieceOnBuilding(Pieces, Fire.PieceIndex);
        }

        if (Fire.bDetached)
        {
            bAnyDetached = true;
        }
        else
        {
            ++AttachedFireCount;
            
            const int32 TargetLevel =
                SurfaceFireStep - Fire.BornFireStep + 1;

            if (TargetLevel != Fire.ClusterLevel)
            {
                Fire.ClusterLevel = TargetLevel;

                Fire.Component->SetVariableInt(SFVFXParameters::ClusterLevel, TargetLevel);
            }
        }
        
    }

    if (!bFinalDestructionApplied)
    {
        const int32 MissingCount = SurfaceFireTarget - AttachedFireCount;

        if (MissingCount > 0)
        {
            AttachedFireCount += AddSurfaceFires(MissingCount, Pieces);
        }
    }

    const bool bNeedsNextTick =
        bAnyDetached ||
        (bFinalDestructionApplied && !SurfaceFires.IsEmpty()) ||
        LastBreakWakeFrame == GFrameCounter;

    if (!bNeedsNextTick)
    {
        GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);
    }
}

void ASFBuildingDestructionActor::RemoveSurfaceFire(FSFSurfaceFire& Fire)
{
    if (!IsValid(Fire.Component))
    {
        return;
    }

    // 조각에서 분리하지 않고 신규 불꽃 생성만 중단한다.
    Fire.Component->SetAutoDestroy(true);
    Fire.Component->Deactivate();
}

bool ASFBuildingDestructionActor::IsPieceShrunk(const FSFPieceState& Pieces, int32 PieceIndex, float RestScale) const
{
    return Pieces.GetScale(PieceIndex) / RestScale < SurfaceFireShrinkThreshold;
}

bool ASFBuildingDestructionActor::IsPieceOnBuilding(const FSFPieceState& Pieces, int32 PieceIndex) const
{
    // 파사드가 없으면 루트가 아직 안 깨졌다는 뜻이다.
    if (!Pieces.State.IsSet())
    {
        return true;
    }

    if (PieceIndex < 0 || PieceIndex >= Pieces.NumTransforms)
    {
        return false;
    }
    
    /*
     * 내부 클러스터에 실려 떨어지는 조각은 HasBrokenOff로 잡히지 않는다.
     * 부모가 트랜스폼 계층에서 빠지면서 조각 자신은 비활성이 되고,
     * HasBrokenOff의 첫 조건인 IsActive가 거짓이 되기 때문이다.
     */
    if (Pieces.State->HasDynamicInternalClusterParent(PieceIndex))
    {
        return false;
    }

    // 현재 물리 계층의 가장 위쪽 부모를 찾는다.
    int32 TopIndex = PieceIndex;

    for (int32 Guard = 0; Guard < Pieces.NumTransforms; ++Guard)
    {
        const int32 ParentIndex =
            GeometryCollectionComponent->GetParent(TopIndex);

        if (ParentIndex == INDEX_NONE)
        {
            break;
        }

        if (ParentIndex < 0 || ParentIndex >= Pieces.NumTransforms)
        {
            return false;
        }

        TopIndex = ParentIndex;
    }

    return !Pieces.State->HasBrokenOff(TopIndex);
}

int32 ASFBuildingDestructionActor::AddSurfaceFires(int32 DesiredCount, const FSFPieceState& Pieces)
{
    if (DesiredCount <= 0 ||
        !IsValid(SurfaceFireEffect) ||
        !IsValid(GeometryCollectionComponent))
    {
        return 0;
    }

    BuildPieceSpotTable();

    if (CachedSurfaceSpotsByLevel.IsEmpty())
    {
        return 0;
    }

    /*
     * PendingTargetClusterDepth를 사용하는 이유:
     * 한 번의 큰 피해로 여러 파괴 단계를 건너뛰더라도
     * 최종적으로 요청된 GC Level의 표면 후보를 사용하기 위해서다.
     *
     * 파괴 단계 진입 전에는 값이 0이므로 Level 0 표면을 사용한다.
     */
    if (PendingTargetClusterDepth > 0 &&
    AppliedClusterDepth < PendingTargetClusterDepth)
    {
        return 0;
    }

    const int32 RequestedGCLevel = FMath::Clamp(
        AppliedClusterDepth,
        0,
        CachedSurfaceSpotsByLevel.Num() - 1);

    const TArray<FSFPieceSpot>* CandidateSpots = nullptr;

    /*
     * 요청 Level에 후보가 없다면 더 낮은 Level부터 찾는다.
     * 더 깊은 Level로 올라가면 아직 노출되지 않은 조각을 선택할 수 있으므로
     * 낮은 방향으로만 폴백한다.
     */
    for (int32 Level = RequestedGCLevel; Level >= 0; --Level)
    {
        if (!CachedSurfaceSpotsByLevel[Level].IsEmpty())
        {
            CandidateSpots = &CachedSurfaceSpotsByLevel[Level];
            break;
        }
    }

    if (!CandidateSpots)
    {
        return 0;
    }

    /*
     * GC Level별 후보군은 이미 분리됐으므로,
     * 반경 값은 단계 이동이 아니라 전체 건물에 대한 연출 필터로만 사용한다.
     */
    const int32 RadialStage =
        FMath::Max(AppliedStageIndex + 1, 1);

    const float RadialShift =
        SurfaceFireRadialRatioStepPerLevel *
        (RadialStage - 1);

    const float LevelMinRadialRatio =
        FMath::Max(
            SurfaceFireMinRadialRatio - RadialShift,
            0.f);

    const float LevelMaxRadialRatio =
        FMath::Max(
            SurfaceFireMaxRadialRatio - RadialShift,
            LevelMinRadialRatio);
    
    const float MinDistSq = FMath::Square(
        CachedRestBounds.GetSize().Size2D() * SurfaceFireMinDistanceRatio);

    int32 Added = 0;

    /*
     * 개수는 반드시 채운다. 조건을 단계적으로 푼다.
     * 0차는 전부 지키고, 1차는 간격을, 2차는 반경 밴드까지 푼다.
     * 붙어 있는 조각인지만은 끝까지 지킨다.
     */
    for (int32 Pass = 0; Pass < 3 && Added < DesiredCount; ++Pass)
    {
        const float PassMinDistSq = (Pass == 0) ? MinDistSq : 0.f;

        const float PassMinRadialRatio = (Pass < 2) ? LevelMinRadialRatio : 0.f;
        const float PassMaxRadialRatio = (Pass < 2) ? LevelMaxRadialRatio : 1.f;
        
        for (const FSFPieceSpot& Spot : *CandidateSpots)
        {
            if (Added >= DesiredCount)
            {
                break;   // 목표를 채우면 남은 조각은 보지 않는다.
            }

            if (Spot.RadialRatio < PassMinRadialRatio ||
                Spot.RadialRatio > PassMaxRadialRatio)
            {
                continue;
            }

            // 같은 조각이거나, 이미 붙은 불과 너무 가까우면 건너뛴다.
            const bool bReject =
                SurfaceFires.ContainsByPredicate(
                    [&Spot, PassMinDistSq](const FSFSurfaceFire& Fire)
                    {
                        if (Fire.bDetached)
                        {
                            return false;
                        }

                        return
                            Fire.PieceIndex == Spot.PieceIndex ||
                            FVector::DistSquared(
                                Fire.LocalLocation, Spot.LocalLocation) < PassMinDistSq;
                    });

            if (bReject)
            {
                continue;
            }

            if (!Pieces.IsValidPiece(Spot.PieceIndex))
            {
                continue;
            }

            if (IsPieceShrunk(Pieces, Spot.PieceIndex, Spot.RestScale) ||
                !IsPieceOnBuilding(Pieces, Spot.PieceIndex))
            {
                continue;
            }
            
            /*
             * UGeometryCollectionComponent::GetSocketTransform()은
             * 해당 Bone의 현재 ComponentSpaceTransform을 이용한다.
             */
            const FTransform BoneWorldTransform =
                GeometryCollectionComponent->GetSocketTransform(
                    Spot.BoneName,RTS_World);

            const FVector WorldLocation =
                BoneWorldTransform.TransformPosition(Spot.BoneLocalLocation);

            UNiagaraComponent* Fire =
                UNiagaraFunctionLibrary::SpawnSystemAttached(
                    SurfaceFireEffect,
                    GeometryCollectionComponent,
                    Spot.BoneName,
                    WorldLocation,
                    FRotator::ZeroRotator,
                    EAttachLocation::KeepWorldPosition,
                    false, // bAutoDestroy
                    false, // bAutoActivate
                    ENCPoolMethod::None,
                    false); // bPreCullCheck
            
            if (IsValid(Fire))
            {
                // 조각의 축소·0 스케일을 상속하면 컴포넌트 바운드가 0으로 붕괴해
                // 파티클이 살아 있어도 컬링된다. 위치·회전만 따라가게 한다.
                Fire->SetUsingAbsoluteScale(true);

                // SpawnSystemAttached가 KeepWorldPosition 보정으로 상대 스케일에
                // 1/부모스케일을 넣어둔다. 절대 스케일에서는 그 값이 그대로
                // 월드 스케일이 되어버리므로 1로 되돌린다.
                Fire->SetRelativeScale3D(FVector::OneVector);
                // 새 불은 1에서 시작한다. 이후 불꽃 구간을 넘을 때마다 오른다.
                Fire->SetVariableInt(SFVFXParameters::ClusterLevel, 1);
                
                Fire->Activate(true);
                
                // AddDefaulted_GetRef = 배열끝에 기본형 요소를 추가하고 참조리턴
                FSFSurfaceFire& NewFire = SurfaceFires.AddDefaulted_GetRef();

                NewFire.Component = Fire;
                NewFire.PieceIndex = Spot.PieceIndex;
                NewFire.bDetached = false;
                NewFire.LocalLocation = Spot.LocalLocation;
                NewFire.RestScale = Spot.RestScale;
                NewFire.ClusterLevel = 1;
                NewFire.BornFireStep = SurfaceFireStep;
                
                ++Added;
            }
        }
    }

    return Added;
}

void ASFBuildingDestructionActor::BuildPieceSpotTable()
{
    if (bPieceSpotsBuilt ||
        !IsValid(GeometryCollectionComponent))
    {
        return;
    }

    const UGeometryCollection* Rest =
        GeometryCollectionComponent->GetRestCollection();

    if (!Rest || !Rest->GetGeometryCollection())
    {
        return;
    }

    bPieceSpotsBuilt = true;
    CachedSurfaceSpotsByLevel.Reset();

    const FGeometryCollection& Collection =
        *Rest->GetGeometryCollection();

    // Rest 상태의 컴포넌트 공간 Transform
    TArray<FTransform3f> RestTransforms;

    GeometryCollectionAlgo::GlobalMatrices(
        Collection.Transform,
        Collection.Parent,
        RestTransforms);

    const TManagedArray<int32>& Parents =
        Collection.Parent;

    const TManagedArray<TSet<int32>>& TChildren =
        Collection.Children;

    const TManagedArray<FBox>& Boxes =
        Collection.BoundingBox;

    const TManagedArray<int32>& TransformIndices =
        Collection.TransformIndex;

    const TManagedArray<FString>& BoneNames =
        Collection.BoneName;

    const TManagedArray<int32>& FaceStarts =
        Collection.FaceStart;

    const TManagedArray<int32>& FaceCounts =
        Collection.FaceCount;
    
    const TManagedArray<bool>& VisibleFaces =
        Collection.Visible;

    const TManagedArray<bool>& InternalFaces =
        Collection.Internal;
    
    /*
     * Proximity는 Geometry Group의 조각 인접 관계다.
     * 에셋에 데이터가 없을 수도 있으므로 FindAttribute를 사용한다.
     */
    const TManagedArray<TSet<int32>>* Proximity =
        Collection.FindAttribute<TSet<int32>>(
            FName(TEXT("Proximity")), FGeometryCollection::GeometryGroup);

    // -------------------------------------------------------
    // 1. 각 Transform의 Rest 계층 Level 계산
    // -------------------------------------------------------

    TArray<int32> HierarchyLevels;

    HierarchyLevels.Init(INDEX_NONE,Parents.Num());

    for (int32 TransformIndex = 0;
         TransformIndex < Parents.Num();
         ++TransformIndex)
    {
        HierarchyLevels[TransformIndex] =
            CalculateRestHierarchyLevel(Parents, TransformIndex);
    }

    // -------------------------------------------------------
    // 2. 리프 Geometry의 Rest 바운드와 외부 표면 여부 캐싱
    // -------------------------------------------------------

    TArray<bool> bLeafGeometry;
    TArray<bool> bHasExternalFace;
    TArray<FBox> PieceBoxes;

    bLeafGeometry.Init(false, Boxes.Num());
    bHasExternalFace.Init(false, Boxes.Num());
    PieceBoxes.Init(FBox(ForceInit), Boxes.Num());

    int32 MaxLeafLevel = 0;

    for (int32 GeometryIndex = 0;
         GeometryIndex < Boxes.Num();
         ++GeometryIndex)
    {
        if (!TransformIndices.IsValidIndex(GeometryIndex))
        {
            continue;
        }

        const int32 TransformIndex =
            TransformIndices[GeometryIndex];

        if (!TChildren.IsValidIndex(TransformIndex) ||
            !RestTransforms.IsValidIndex(TransformIndex) ||
            !HierarchyLevels.IsValidIndex(TransformIndex))
        {
            continue;
        }

        // 실제 메쉬를 가진 최하위 리프 조각만 사용한다.
        if (TChildren[TransformIndex].Num() != 0)
        {
            continue;
        }

        bLeafGeometry[GeometryIndex] = true;

        PieceBoxes[GeometryIndex] =
            Boxes[GeometryIndex].TransformBy(
                FTransform(RestTransforms[TransformIndex]));

        MaxLeafLevel = FMath::Max(
            MaxLeafLevel,HierarchyLevels[TransformIndex]);

        /*
         * Internal == false인 Face가 하나라도 있으면
         * 원래 건물 외부 표면을 포함한 조각이다.
         */
        if (FaceStarts.IsValidIndex(GeometryIndex) &&
            FaceCounts.IsValidIndex(GeometryIndex))
        {
            const int32 FaceBegin = FaceStarts[GeometryIndex];

            const int32 FaceEnd =
                FaceBegin + FMath::Max(FaceCounts[GeometryIndex],0);

            for (int32 FaceIndex = FaceBegin; FaceIndex < FaceEnd; ++FaceIndex)
            {
                if (VisibleFaces.IsValidIndex(FaceIndex) &&
                    VisibleFaces[FaceIndex] &&
                    InternalFaces.IsValidIndex(FaceIndex) &&
                    !InternalFaces[FaceIndex])
                {
                    bHasExternalFace[GeometryIndex] = true;
                    break;
                }
            }
        }
    }

    CachedSurfaceSpotsByLevel.SetNum(MaxLeafLevel + 1);

    const float MinZ =
        CachedRestBounds.Min.Z + CachedRestBounds.GetSize().Z * SurfaceFireMinHeightRatio;

    // -------------------------------------------------------
    // 3. GC Level별 표면 조각 후보 생성
    // -------------------------------------------------------

    for (int32 GCLevel = 0; GCLevel <= MaxLeafLevel; ++GCLevel)
    {
        /*
         * 같은 Level에서 각 클러스터가 포함한 리프 조각들의
         * 전체 Rest 바운드를 계산한다.
         */
        TMap<int32, FBox> ClusterBoundsByTransform;

        for (int32 GeometryIndex = 0; GeometryIndex < Boxes.Num(); ++GeometryIndex)
        {
            if (!bLeafGeometry[GeometryIndex])
            {
                continue;
            }

            const int32 TransformIndex = TransformIndices[GeometryIndex];

            const int32 ClusterIndex =
                FindAncestorAtRestLevel(
                    Parents,
                    HierarchyLevels,
                    TransformIndex,
                    GCLevel);

            if (ClusterIndex == INDEX_NONE)
            {
                continue;
            }

            if (FBox* ExistingBounds =
                    ClusterBoundsByTransform.Find(ClusterIndex))
            {
                *ExistingBounds +=
                    PieceBoxes[GeometryIndex];
            }
            else
            {
                ClusterBoundsByTransform.Add(
                    ClusterIndex, PieceBoxes[GeometryIndex]);
            }
        }

        TArray<FSFPieceSpot>& LevelSpots =
            CachedSurfaceSpotsByLevel[GCLevel];

        for (int32 GeometryIndex = 0;
             GeometryIndex < Boxes.Num(); ++GeometryIndex)
        {
            if (!bLeafGeometry[GeometryIndex])
            {
                continue;
            }

            const int32 TransformIndex =
                TransformIndices[GeometryIndex];

            if (!BoneNames.IsValidIndex(TransformIndex))
            {
                continue;
            }

            const FName BoneName(*BoneNames[TransformIndex]);

            if (BoneName.IsNone())
            {
                continue;
            }

            const FBox& PieceBox =
                PieceBoxes[GeometryIndex];

            const FVector PieceLocation =
                PieceBox.GetCenter();

            if (PieceLocation.Z < MinZ)
            {
                continue;
            }

            const int32 ClusterIndex =
                FindAncestorAtRestLevel(
                    Parents,
                    HierarchyLevels,
                    TransformIndex,
                    GCLevel);

            const FBox* ClusterBounds =
                ClusterBoundsByTransform.Find(ClusterIndex);

            if (!ClusterBounds)
            {
                continue;
            }

            /*
             * 원래 외부 표면을 포함한 조각인지 확인한다.
             * 생성 시 외부 면에 붙일지 절단면에 붙일지는 구분하지 않는다.
             */
            const bool bOriginalExterior =
                bHasExternalFace[GeometryIndex];

            /*
             * 현재 GC Level에서 다른 클러스터와 맞닿아 있는 조각은
             * 해당 Level이 파괴됐을 때 표면 조각이 된다.
             */
            bool bLevelBoundary = false;

            const bool bHasProximityData =
                Proximity &&
                Proximity->IsValidIndex(GeometryIndex) &&
                !(*Proximity)[GeometryIndex].IsEmpty();

            if (bHasProximityData)
            {
                for (const int32 NeighborGeometryIndex :
                     (*Proximity)[GeometryIndex])
                {
                    if (!TransformIndices.IsValidIndex(NeighborGeometryIndex))
                    {
                        continue;
                    }

                    const int32 NeighborTransformIndex =
                        TransformIndices[NeighborGeometryIndex];

                    const int32 NeighborClusterIndex =
                        FindAncestorAtRestLevel(
                            Parents,
                            HierarchyLevels,
                            NeighborTransformIndex,
                            GCLevel);

                    if (NeighborClusterIndex != INDEX_NONE &&
                        NeighborClusterIndex != ClusterIndex)
                    {
                        bLevelBoundary = true;
                        break;
                    }
                }
            }

            /*
             * Proximity가 없는 GC 에셋에서만 AABB 판정을
             * 보조 수단으로 사용한다.
             */
            bool bFallbackSurface = false;

            if (!bHasProximityData)
            {
                const float SurfaceGapRatio =
                    GetBoxSurfaceGapRatio(
                        *ClusterBounds,
                        PieceBox);

                bFallbackSurface =
                    SurfaceGapRatio <=
                    SurfaceFireClusterSurfaceTolerance;
            }

            const bool bSurfacePiece =
                bOriginalExterior ||
                bLevelBoundary ||
                bFallbackSurface;

            if (!bSurfacePiece)
            {
                continue;
            }

            /*
             * 조각 위치를 기준으로 기존 전역 반경 범위를 계산한다.
             */
            FVector RadialDirection =
                PieceLocation -
                CachedRestBounds.GetCenter();

            RadialDirection.Z = 0.f;

            const float HorizontalDistance =
                RadialDirection.Size2D();

            float RadialRatio = 0.f;

            if (HorizontalDistance > KINDA_SMALL_NUMBER)
            {
                RadialDirection /= HorizontalDistance;

                RadialRatio =
                    HorizontalDistance /
                    FMath::Max(
                        RayBoxDistance2D(
                            CachedRestBounds.GetExtent(),
                            RadialDirection),
                        1.f);
            }

            const FTransform PieceToComponent(
                RestTransforms[TransformIndex]);

            const FVector SurfaceFirePoint =
                GetSurfaceFirePoint(
                    *ClusterBounds,
                    CachedRestBounds,
                    PieceBox,
                    SurfaceFireSurfaceRatio);

            FSFPieceSpot& NewSpot =
                LevelSpots.AddDefaulted_GetRef();

            NewSpot.LocalLocation =
                SurfaceFirePoint;

            NewSpot.BoneLocalLocation =
                PieceToComponent.InverseTransformPosition(
                    SurfaceFirePoint);
            
            NewSpot.PieceIndex = TransformIndex;

            NewSpot.BoneName = BoneName;

            NewSpot.RestScale =
                FMath::Max(
                    RestTransforms[TransformIndex]
                        .GetScale3D()
                        .GetAbsMax(),
                    KINDA_SMALL_NUMBER);

            NewSpot.RadialRatio = RadialRatio;
        }

        // Level별 후보 순서를 초기화 시 한 번만 섞는다.
        for (int32 Index = 0;
             Index < LevelSpots.Num() - 1; ++Index)
        {
            LevelSpots.Swap(
                Index,
                FMath::RandRange(
                    Index,
                    LevelSpots.Num() - 1));
        }

        LevelSpots.Shrink();

        UE_LOG(LogTemp, Verbose,
            TEXT("[%s] GC surface fire Level %d: %d candidates"),
            *GetName(),
            GCLevel,
            LevelSpots.Num());
    }
}

float ASFBuildingDestructionActor::GetComponentUniformScale() const
{
    return FMath::Max(
            GeometryCollectionComponent->GetComponentTransform().GetScale3D().GetAbsMax(),
            KINDA_SMALL_NUMBER);
}
