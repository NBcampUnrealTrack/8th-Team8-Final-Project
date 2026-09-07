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
    else
    {
        StartSurfaceFireMaintenance();
    }

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
        !bFinalDestructionApplied ||
        BreakEvent.Component != GeometryCollectionComponent)
    {
        return;
    }

    // 새롭게 활성화된 하위 클러스터까지 완전히 해체한다.
    GeometryCollectionComponent->CrumbleActiveClusters();
}

void ASFBuildingDestructionActor::StartSurfaceFireMaintenance()
{
    if (bFinalDestructionApplied ||
        !IsValid(GeometryCollectionComponent) ||
        !IsValid(SurfaceFireEffect))
    {
        return;
    }

    // 첫 불꽃은 기다리지 않고 즉시 생성한다.
    UpdateSurfaceFires();

    if (GetWorldTimerManager().IsTimerActive(
        SurfaceFireTimerHandle))
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        SurfaceFireTimerHandle,
        this,
        &ThisClass::UpdateSurfaceFires,
        SurfaceFireUpdateInterval,
        true);
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

    int32 AttachedFireCount = 0;

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
            PieceTransforms[Fire.PieceIndex]
                .GetScale3D()
                .GetAbsMax();

        if (PieceScale < SurfaceFireShrinkThreshold)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Surface fire removed: Piece=%d Scale=%.3f Final=%s"),
                *GetName(), Fire.PieceIndex, PieceScale,
                    bFinalDestructionApplied
                    ? TEXT("true")
                    : TEXT("false"));

            RemoveSurfaceFire(Fire);
            SurfaceFires.RemoveAtSwap(Index);
            continue;
        }

        if (!Fire.bDetached)
        {
            Fire.bDetached = !IsPieceAttached(Fire.PieceIndex);
        }

        if (!Fire.bDetached)
        {
            ++AttachedFireCount;
        }
    }

    if (!bFinalDestructionApplied)
    {
        const int32 MissingCount =
            GetTargetSurfaceFireCount() - AttachedFireCount;

        if (MissingCount > 0)
        {
            const int32 AddedCount = AddSurfaceFires(MissingCount);

            /*
             * 불꽃도 없고 유효한 조각도 없다면
             * 계속 전체 GC를 순회하지 않는다.
             *
             * 다음 손상 단계가 적용되면
             * StartSurfaceFireMaintenance()에서 다시 시작된다.
             */
            if (AddedCount == 0 && SurfaceFires.IsEmpty())
            {
                GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);
            }
        }
    }
    else if (SurfaceFires.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(SurfaceFireTimerHandle);
    }
}

bool ASFBuildingDestructionActor::IsPieceAttached(int32 PieceIndex) const
{
    if (!IsValid(GeometryCollectionComponent))
    {
        return false;
    }

    FGeometryDynamicCollection* DynamicCollection =
        GeometryCollectionComponent->GetDynamicCollection();

    if (!DynamicCollection ||
        PieceIndex < 0 ||
        PieceIndex >= DynamicCollection->GetNumTransforms())
    {
        return false;
    }

    // 루트가 아직 깨지지 않았다면 모든 조각은 건물 본체 소속이다.
    if (!GeometryCollectionComponent->IsRootBroken())
    {
        return true;
    }

    FGeometryCollectionDynamicStateFacade DynamicState(*DynamicCollection);

    if (!DynamicState.IsValid())
    {
        return false;
    }

    // 현재 물리 계층의 가장 위쪽 부모를 찾는다.
    int32 TopIndex = PieceIndex;

    for (int32 Guard = 0; Guard < DynamicCollection->GetNumTransforms(); ++Guard)
    {
        const int32 ParentIndex =
            GeometryCollectionComponent->GetParent(TopIndex);

        if (ParentIndex == INDEX_NONE)
        {
            break;
        }

        if (ParentIndex < 0 ||
            ParentIndex >= DynamicCollection->GetNumTransforms())
        {
            return false;
        }

        TopIndex = ParentIndex;
    }

    /*
     * 최상위 조각이 Dynamic/Sleeping 상태로 원래 부모에서
     * 떨어졌다면 건물 본체가 아니라 낙하 파편이다.
     */
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

    TArray<FSFPieceSpot> Spots;
    CollectAttachedPieceSpots(Spots);

    if (Spots.IsEmpty())
    {
        return 0;
    }

    // 후보 전체를 한 번 섞어둔다. 이후에는 앞에서부터 훑기만 한다.
    for (int32 i = 0; i < Spots.Num() - 1; ++i)
    {
        Spots.Swap(i, FMath::RandRange(i, Spots.Num() - 1));
    }

    const float MinDistSq = FMath::Square(
        CachedRestBounds.GetSize().Size2D() * SurfaceFireMinDistanceRatio);

    const FTransform ComponentTransform =
        GeometryCollectionComponent->GetComponentTransform();

    int32 Added = 0;

    // 1차는 간격을 지켜서, 자리가 모자라면 2차에서 간격을 풀고 개수를 채운다.
    for (int32 Pass = 0; Pass < 2 && Added < DesiredCount; ++Pass)
    {
        const float PassMinDistSq = (Pass == 0) ? MinDistSq : 0.f;

        for (const FSFPieceSpot& Spot : Spots)
        {
            if (Added >= DesiredCount)
            {
                break;
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
                    true);

            // 교체
            #if !UE_BUILD_SHIPPING
            DrawDebugSphere(GetWorld(), WorldLocation, 40.f, 12,
                FColor::Red, false, 10.f, 0, 2.f);
            #endif
            
            if (IsValid(Fire))
            {
                // 조각의 축소·0 스케일을 상속하면 컴포넌트 바운드가 0으로 붕괴해
                // 파티클이 살아 있어도 컬링된다. 위치·회전만 따라가게 한다.
                Fire->SetUsingAbsoluteScale(true);

                // SpawnSystemAttached가 KeepWorldPosition 보정으로 상대 스케일에
                // 1/부모스케일을 넣어둔다. 절대 스케일에서는 그 값이 그대로
                // 월드 스케일이 되어버리므로 1로 되돌린다.
                Fire->SetRelativeScale3D(FVector::OneVector);

                FSFSurfaceFire& NewFire = SurfaceFires.AddDefaulted_GetRef();

                NewFire.Component = Fire;
                NewFire.PieceIndex = Spot.PieceIndex;
                NewFire.bDetached = false;
                NewFire.LocalLocation = Spot.LocalLocation;

                ++Added;
            }
        }
    }

    return Added;
}

void ASFBuildingDestructionActor::CollectAttachedPieceSpots(TArray<FSFPieceSpot>& OutSpots)
{
    OutSpots.Reset();

    if (!IsValid(GeometryCollectionComponent))
    {
        return;
    }

    const TArray<FTransform3f>& PieceTransforms =
        GeometryCollectionComponent->GetComponentSpaceTransforms3f();

    if (PieceTransforms.Num() == 0)
    {
        return;
    }

    const UGeometryCollection* Rest =
        GeometryCollectionComponent->GetRestCollection();

    if (!Rest || !Rest->GetGeometryCollection())
    {
        return;
    }

    const FGeometryCollection& Collection = *Rest->GetGeometryCollection();

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

        if (!TChildren.IsValidIndex(XfIdx))        continue;
        if (TChildren[XfIdx].Num() != 0)           continue;   // 리프 조각만
        if (!PieceTransforms.IsValidIndex(XfIdx)) continue;
        
        /*
     * Remove On Break로 축소 중이거나 이미 사라진 조각은
     * 새로운 불꽃 후보로 다시 사용하지 않는다.
     */
        const float PieceScale =
            PieceTransforms[XfIdx]
                .GetScale3D()
                .GetAbsMax();

        if (PieceScale < SurfaceFireShrinkThreshold)
        {
            continue;
        }
        
        if (!BoneNames.IsValidIndex(XfIdx))
        {
            continue;
        }

        const FName BoneName(*BoneNames[XfIdx]);

        if (BoneName.IsNone())
        {
            continue;
        }
        
        if (!IsPieceAttached(XfIdx))
        {
            continue;
        }
        
        // 이미 불이 붙어 있는 조각 제외
        const bool bAlreadyBurning = SurfaceFires.ContainsByPredicate(
            [XfIdx](const FSFSurfaceFire& Fire)
            {
                return Fire.PieceIndex == XfIdx;
            });

        if (bAlreadyBurning)
        {
            continue;
        }
        
        // Spawn Location
        // 조각 AABB를 컴포넌트 공간으로 옮긴다. 회전이 반영된 축정렬 박스가 나온다.
        const FBox PieceBox =
            Boxes[GeoIdx].TransformBy(FTransform(PieceTransforms[XfIdx]));

        FVector LocalCenter = PieceBox.GetCenter();

        if (LocalCenter.Z < MinZ)
        {
            continue;
        }

        // 건물 중심에서 이 조각을 향하는 수평 방향 = 바깥쪽
        FVector Outward = LocalCenter - CachedRestBounds.GetCenter();
        Outward.Z = 0.f;
        Outward = Outward.GetSafeNormal();

        // 방향이 나오지 않는 조각 = 건물 한가운데.
        // 표면을 특정할 수 없고 밖에서 보이지도 않으므로 후보에서 제외한다.
        if (Outward.IsNearlyZero())
        {
            continue;
        }

        // Outward 방향으로 중심에서 표면까지의 거리(AABB 지원 함수).
        // 조각 크기에서 직접 나오므로 건물 스케일에 자동으로 따라간다.
        const FVector Extent = PieceBox.GetExtent() * 0.6f;

        LocalCenter += Outward *
            (FMath::Abs(Outward.X) * Extent.X +
             FMath::Abs(Outward.Y) * Extent.Y);

        FSFPieceSpot& NewSpot = OutSpots.AddDefaulted_GetRef();

        NewSpot.LocalLocation = LocalCenter;
        NewSpot.PieceIndex = XfIdx;
        NewSpot.BoneName = BoneName;
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
