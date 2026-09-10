#include "Buildings/GC/SFBuildingDestructionActor.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "GeometryCollectionProxyData.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

namespace SFVFXParameters
{
    const FName ClusterLevel(TEXT("User.ClusterLevel"));
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
    GeometryCollectionComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    GeometryCollectionComponent->SetSimulatePhysics(false);
    GeometryCollectionComponent->SetEnableGravity(true);
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

bool ASFBuildingDestructionActor::InitializeDestruction(
    const UGeometryCollection* InCollection)
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
    
    GeometryCollectionComponent->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);

    GeometryCollectionComponent->SetEnableGravity(true);
    GeometryCollectionComponent->SetSimulatePhysics(true);
    GeometryCollectionComponent->SetVisibility(true, true);

    // Asset Default 적용 이후에도 명시적으로 보장한다.
    GeometryCollectionComponent->bIgnoreRadialImpulse = false;
    GeometryCollectionComponent->bIgnoreRadialForce = false;

    bInitialized = true;
    return true;
}

void ASFBuildingDestructionActor::ApplyDamageStage(int32 StageIndex, const FBuildingDestruction& StageData)
{
    if (!bInitialized ||
        bFinalDestructionApplied ||
        StageIndex < 0 ||
        StageIndex <= AppliedStageIndex)
    {
        return;
    }

    AppliedStageIndex = StageIndex;
    
    ApplyStageSettings(StageData,StageIndex + 1);
    
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Destruction stage applied: %d"),
        *GetName(), StageIndex);

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

    /**
     * BP StaticMeshComponent가 2,2,2 Scale이면
     * 파괴 반경도 그에 맞춰 증가시킨다.
     */
    const float ComponentScale =
        FMath::Max(ComponentTransform.GetScale3D().GetAbsMax(),
                KINDA_SMALL_NUMBER);

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

    if (bFinalDestructionApplied)
    {
        // 새롭게 활성화된 하위 클러스터까지 완전히 해체한다.
        GeometryCollectionComponent->CrumbleActiveClusters();
    }

    /*
     * IsTimerActive만으로는 부족하다.
     * UpdateSurfaceFires가 같은 호출 안에서 타이머를 도로 끄기 때문에
     * 뒤이어 도착한 Break가 가드를 그대로 통과한다.
     */
    if (LastBreakWakeFrame == GFrameCounter)
    {
        return;
    }

    LastBreakWakeFrame = GFrameCounter;

    if (!GetWorldTimerManager().IsTimerActive(SurfaceFireTimerHandle))
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
    
    if (GetWorldTimerManager().IsTimerActive(
        SurfaceFireTimerHandle))
    {
        return;
    }
    
    // 루프를 먼저 걸고 즉시 한 번 갱신한다.
    // 할 일이 없으면 UpdateSurfaceFires가 스스로 이 타이머를 끈다.
    GetWorldTimerManager().SetTimer(
        SurfaceFireTimerHandle,
        this,
        &ThisClass::UpdateSurfaceFires,
        SurfaceFireUpdateInterval,
        true);
    
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

    const TArray<FTransform3f>& PieceTransforms =
        GeometryCollectionComponent->GetComponentSpaceTransforms3f();

    FGeometryDynamicCollection* Dynamic =
        GeometryCollectionComponent->GetDynamicCollection();

    // 루트가 멀쩡하면 모든 조각이 본체 소속이다. 파사드 자체가 필요 없다.
    const bool bRootIntact =
        !Dynamic || !GeometryCollectionComponent->IsRootBroken();

    TOptional<FGeometryCollectionDynamicStateFacade> DynamicState;

    if (!bRootIntact)
    {
        DynamicState.Emplace(*Dynamic);

        if (!DynamicState->IsValid())
        {
            DynamicState.Reset();
        }
    }

    const int32 NumTransforms = Dynamic ? Dynamic->GetNumTransforms() : 0;

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

        if (!PieceTransforms.IsValidIndex(Fire.PieceIndex))
        {
            RemoveSurfaceFire(Fire);
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }

        /*
         * 계층 분리 판정보다 먼저 검사한다.
         * 빠른 파괴로 분리 상태를 놓쳐도 조각 축소는 감지할 수 있다.
         */
        const float PieceScale =
            PieceTransforms[Fire.PieceIndex].GetScale3D().GetAbsMax();

        if (PieceScale / Fire.RestScale < SurfaceFireShrinkThreshold)
        {
            RemoveSurfaceFire(Fire);
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }

        if (!Fire.bDetached && DynamicState.IsSet())
        {
            Fire.bDetached = !IsPieceAttached(
                *DynamicState, NumTransforms, Fire.PieceIndex);
        }

        if (Fire.bDetached)
        {
            bAnyDetached = true;
        }
        else
        {
            ++AttachedFireCount;
            
            // /*
            //  * 분리 판정을 실제로 돌린 패스에서만 레벨을 맞춘다.
            //  * 물리가 아직 틱하지 않은 패스에서는 잔해도 붙어 있는 것으로 보인다.
            //  */
            // if (DynamicState.IsSet())
            // {
            //     const int32 TargetLevel = AppliedStageIndex - Fire.BornStageIndex + 1;
            //
            //     if (TargetLevel != Fire.ClusterLevel)
            //     {
            //         Fire.ClusterLevel = TargetLevel;
            //
            //         Fire.Component->SetVariableInt(SFVFXParameters::ClusterLevel, TargetLevel);
            //     }
            // }
        }
    }

    if (!bFinalDestructionApplied)
    {
        const int32 MissingCount =
            GetTargetSurfaceFireCount() - AttachedFireCount;

        if (MissingCount > 0)
        {
            AddSurfaceFires(MissingCount);
        }
    }

    /*
     * Break 디스패치 중에 돈 갱신은 한 번 더 돌린다.
     * 같은 프레임의 나머지 Break를 프레임 가드가 막기 때문에,
     * 여기서 잠들면 그 프레임에 떨어진 조각을 아무도 다시 보지 않는다.
     */
    const bool bNeedsNextTick =
        bAnyDetached ||
        (bFinalDestructionApplied && !SurfaceFires.IsEmpty()) ||
        LastBreakWakeFrame == GFrameCounter;

    if (!bNeedsNextTick)
    {
        GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);
    }
}

bool ASFBuildingDestructionActor::IsPieceAttached(
        const FGeometryCollectionDynamicStateFacade& DynamicState,
        int32 NumTransforms,
        int32 PieceIndex) const
{
    if (PieceIndex < 0 || PieceIndex >= NumTransforms)
    {
        return false;
    }
    /*
         * 내부 클러스터에 실려 떨어지는 조각은 HasBrokenOff로 잡히지 않는다.
         * 부모가 트랜스폼 계층에서 빠지면서 조각 자신은 비활성이 되고,
         * HasBrokenOff의 첫 조건인 IsActive가 거짓이 되기 때문이다.
         */
    if (DynamicState.HasDynamicInternalClusterParent(PieceIndex))
    {
        return false;
    }
    // 현재 물리 계층의 가장 위쪽 부모를 찾는다.
    int32 TopIndex = PieceIndex;

    for (int32 Guard = 0; Guard < NumTransforms; ++Guard)
    {
        const int32 ParentIndex =
            GeometryCollectionComponent->GetParent(TopIndex);

        if (ParentIndex == INDEX_NONE)
        {
            break;
        }

        if (ParentIndex < 0 || ParentIndex >= NumTransforms)
        {
            return false;
        }

        TopIndex = ParentIndex;
    }

    return !DynamicState.HasBrokenOff(TopIndex);
}

int32 ASFBuildingDestructionActor::GetTargetSurfaceFireCount() const
{
    if (!IsValid(GeometryCollectionComponent))
    {
        return SurfaceFireMinCount;
    }

    // ApplyStageSettings(:171)의 반경 계산과 같은 방식으로 스케일을 반영한다.
    const float ComponentScale = FMath::Max(
        GeometryCollectionComponent->GetComponentTransform().GetScale3D().GetAbsMax(),
        KINDA_SMALL_NUMBER);

    const float WorldDiagonal =
        CachedRestBounds.GetSize().Size2D() * ComponentScale;

    return FMath::Clamp(
        FMath::RoundToInt(WorldDiagonal / FMath::Max(SurfaceFireSpacing, 1.f)),
        SurfaceFireMinCount,
        SurfaceFireMaxCount);
}

int32 ASFBuildingDestructionActor::AddSurfaceFires(int32 DesiredCount)
{
    if (DesiredCount <= 0 ||
        !IsValid(SurfaceFireEffect) ||
        !IsValid(GeometryCollectionComponent))
    {
        return 0;
    }

    BuildPieceSpotTable();

    if (CachedPieceSpots.IsEmpty())
    {
        return 0;
    }

    const TArray<FTransform3f>& PieceTransforms =
        GeometryCollectionComponent->GetComponentSpaceTransforms3f();

    FGeometryDynamicCollection* Dynamic =
        GeometryCollectionComponent->GetDynamicCollection();

    const bool bRootIntact =
        !Dynamic || !GeometryCollectionComponent->IsRootBroken();

    TOptional<FGeometryCollectionDynamicStateFacade> DynamicState;

    if (!bRootIntact)
    {
        DynamicState.Emplace(*Dynamic);

        if (!DynamicState->IsValid())
        {
            DynamicState.Reset();
        }
    }

    const int32 NumTransforms = Dynamic ? Dynamic->GetNumTransforms() : 0;

    const float MinDistSq = FMath::Square(
        CachedRestBounds.GetSize().Size2D() * SurfaceFireMinDistanceRatio);

    const FTransform ComponentTransform =
        GeometryCollectionComponent->GetComponentTransform();
    
    // 이번 단계의 불꽃 레벨. 단계 0이 1, 단계 1이 2다.
    const int32 SpawnClusterLevel = FMath::Max(AppliedStageIndex + 1, 1);
    const float LevelMaxRadialRatio = FMath::Max(
    SurfaceFireMaxRadialRatio - 0.15 * (SpawnClusterLevel - 1), SurfaceFireMinRadialRatio);
    
    int32 Added = 0;

    // 1차는 간격을 지켜서, 자리가 모자라면 2차에서 간격을 풀고 개수를 채운다.
    for (int32 Pass = 0; Pass < 2 && Added < DesiredCount; ++Pass)
    {
        const float PassMinDistSq = (Pass == 0) ? MinDistSq : 0.f;

        for (const FSFPieceSpot& Spot : CachedPieceSpots)
        {
            if (Added >= DesiredCount)
            {
                break;   // 목표를 채우면 남은 조각은 보지 않는다.
            }
            // 이번 레벨의 밴드를 벗어난 자리는 건너뛴다.
            if (Spot.RadialRatio > LevelMaxRadialRatio)
            {
                continue;
            }
            // 같은 조각이거나, 이미 붙은 불과 너무 가까우면 건너뛴다.
            const bool bReject =
                SurfaceFires.ContainsByPredicate(
                    [&Spot, PassMinDistSq](
                        const FSFSurfaceFire& Fire)
                    {
                        if (Fire.bDetached)
                        {
                            return false;
                        }

                        return
                            Fire.PieceIndex == Spot.PieceIndex ||
                            FVector::DistSquared(
                                Fire.LocalLocation,
                                Spot.LocalLocation) <
                            PassMinDistSq;
                    });

            if (bReject)
            {
                continue;
            }

            if (!PieceTransforms.IsValidIndex(Spot.PieceIndex))
            {
                continue;
            }

            if (PieceTransforms[Spot.PieceIndex].GetScale3D().GetAbsMax() /
                Spot.RestScale < SurfaceFireShrinkThreshold)
            {
                continue;
            }

            if (DynamicState.IsSet() &&
                !IsPieceAttached(*DynamicState, NumTransforms, Spot.PieceIndex))
            {
                continue;
            }
            
            FVector WorldLocation =
                ComponentTransform.TransformPosition(Spot.LocalLocation);
            
            UNiagaraComponent* Fire =
                UNiagaraFunctionLibrary::SpawnSystemAttached(
                    SurfaceFireEffect,
                    GeometryCollectionComponent,
                    Spot.BoneName,
                    WorldLocation,
                    FRotator::ZeroRotator,
                    EAttachLocation::KeepWorldPosition,
                    false,
                    false);
            
            if (IsValid(Fire))
            {
                // 조각의 축소·0 스케일을 상속하면 컴포넌트 바운드가 0으로 붕괴해
                // 파티클이 살아 있어도 컬링된다. 위치·회전만 따라가게 한다.
                Fire->SetUsingAbsoluteScale(true);

                // SpawnSystemAttached가 KeepWorldPosition 보정으로 상대 스케일에
                // 1/부모스케일을 넣어둔다. 절대 스케일에서는 그 값이 그대로
                // 월드 스케일이 되어버리므로 1로 되돌린다.
                Fire->SetRelativeScale3D(FVector::OneVector);
                // 활성화 전에 넣어야 첫 스폰 버스트부터 반영된다.
                Fire->SetVariableInt(SFVFXParameters::ClusterLevel, SpawnClusterLevel);

                Fire->Activate(true);
                
                // AddDefaulted_GetRef = 배열끝에 기본형 요소를 추가하고 참조리턴
                FSFSurfaceFire& NewFire = SurfaceFires.AddDefaulted_GetRef();

                NewFire.Component = Fire;
                NewFire.PieceIndex = Spot.PieceIndex;
                NewFire.bDetached = false;
                NewFire.LocalLocation = Spot.LocalLocation;
                NewFire.RestScale = Spot.RestScale;
                //NewFire.BornStageIndex = AppliedStageIndex;
                
                ++Added;
            }
        }
    }

    return Added;
}

void ASFBuildingDestructionActor::BuildPieceSpotTable()
{
    if (bPieceSpotsBuilt || !IsValid(GeometryCollectionComponent))
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
    
    const FGeometryCollection& Collection = *Rest->GetGeometryCollection();

    // rest 기준 컴포넌트 공간 변환. 물리 상태와 무관하게 항상 같은 값이다.
    TArray<FTransform3f> RestTransforms;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection.Transform, Collection.Parent, RestTransforms);

    const TManagedArray<FBox>&        Boxes     = Collection.BoundingBox;
    const TManagedArray<int32>&       XfIndices = Collection.TransformIndex;
    const TManagedArray<TSet<int32>>& TChildren = Collection.Children;
    const TManagedArray<FString>&     BoneNames = Collection.BoneName;

    const float MinZ =
        CachedRestBounds.Min.Z +
        CachedRestBounds.GetSize().Z * SurfaceFireMinHeightRatio;

    for (int32 GeoIdx = 0; GeoIdx < Boxes.Num(); ++GeoIdx)
    {
        const int32 XfIdx = XfIndices[GeoIdx];

        if (!TChildren.IsValidIndex(XfIdx))      continue;
        if (TChildren[XfIdx].Num() != 0)         continue;   // 리프 조각만
        if (!RestTransforms.IsValidIndex(XfIdx)) continue;
        if (!BoneNames.IsValidIndex(XfIdx))      continue;

        const FName BoneName(*BoneNames[XfIdx]);

        if (BoneName.IsNone())
        {
            continue;
        }

        const FBox PieceBox =
            Boxes[GeoIdx].TransformBy(FTransform(RestTransforms[XfIdx]));

        FVector LocalCenter = PieceBox.GetCenter();

        if (LocalCenter.Z < MinZ)
        {
            continue;
        }

        FVector Outward = LocalCenter - CachedRestBounds.GetCenter();
        Outward.Z = 0.f;
        Outward = Outward.GetSafeNormal();

        if (Outward.IsNearlyZero())
        {
            continue;
        }

        /*
         * 중심에서 Outward 방향으로 바운드 경계까지의 거리.
         * 축마다 따로 재서 가장 먼저 닿는 면을 쓴다.
         * 모서리 방향에서는 이 값이 커지므로 코너 조각도 공평하게 평가된다.
         */
        const FVector BoundExtent = CachedRestBounds.GetExtent();

        float BoundaryDistance = TNumericLimits<float>::Max();

        if (FMath::Abs(Outward.X) > KINDA_SMALL_NUMBER)
        {
            BoundaryDistance = FMath::Min(
                BoundaryDistance, BoundExtent.X / FMath::Abs(Outward.X));
        }

        if (FMath::Abs(Outward.Y) > KINDA_SMALL_NUMBER)
        {
            BoundaryDistance = FMath::Min(
                BoundaryDistance, BoundExtent.Y / FMath::Abs(Outward.Y));
        }

        const float RadialRatio =
            FVector(LocalCenter - CachedRestBounds.GetCenter()).Size2D() /
            FMath::Max(BoundaryDistance, 1.f);

        // 안쪽 조각과 튀어나온 조각을 동시에 잘라낸다.
        if (RadialRatio < SurfaceFireMinRadialRatio)
        {
            continue;
        }
        
        const FVector Extent = PieceBox.GetExtent() * 0.6f;

        LocalCenter += Outward *
            FVector(FMath::Abs(Outward.X) * Extent.X,
             FMath::Abs(Outward.Y) * Extent.Y,
                 FMath::Abs(Outward.Z) * Extent.Z);

        FSFPieceSpot& NewSpot = CachedPieceSpots.AddDefaulted_GetRef();

        NewSpot.LocalLocation = LocalCenter;
        NewSpot.PieceIndex = XfIdx;
        NewSpot.BoneName = BoneName;
        NewSpot.RestScale = FMath::Max(
            RestTransforms[XfIdx].GetScale3D().GetAbsMax(), KINDA_SMALL_NUMBER);
        NewSpot.RadialRatio = RadialRatio;
    }

    // 순서를 여기서 한 번만 섞는다. 갱신마다 다시 섞지 않는다.
    for (int32 i = 0; i < CachedPieceSpots.Num() - 1; ++i)
    {
        CachedPieceSpots.Swap(
            i, FMath::RandRange(i, CachedPieceSpots.Num() - 1));
    }

    CachedPieceSpots.Shrink();
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

void ASFBuildingDestructionActor::ApplyFinalDestruction(const int32 FinalStageIndex, const FBuildingDestruction& StageData)
{
    if (!bInitialized ||
         bFinalDestructionApplied ||
         FinalStageIndex < 0)
    {
        return;
    }

    // 새 불꽃 보충만 금지한다.
    bFinalDestructionApplied = true;

    GeometryCollectionComponent->RemoveAllAnchors();

    ApplyStageSettings(StageData,FinalStageIndex + 1);
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
