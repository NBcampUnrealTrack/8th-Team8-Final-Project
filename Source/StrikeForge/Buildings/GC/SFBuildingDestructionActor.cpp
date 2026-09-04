#include "Buildings/GC/SFBuildingDestructionActor.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "TimerManager.h"

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[%s] Building destruction initialization failed."),
            *GetName());

        return false;
    }
    
    GeometryCollectionComponent->SetRestCollection(
    InCollection,
    true);
    
    // 파괴 radius의 위치와 크기
    const FBox LocalBounds =
    GeometryCollectionComponent->GetLocalBounds();

    if (LocalBounds.IsValid)
    {
        CachedLocalBoundsCenter = LocalBounds.GetCenter();
        CachedLocalBoundsRadius = FMath::Max(
            LocalBounds.GetExtent().Size(),
            1.f);
        CachedRestBounds = LocalBounds;              // ← 추가
    }
    
    
    GeometryCollectionComponent->bAllowRemovalOnBreak = true;
    
    // 단계별 파괴 실험 중에는 Damage 전파 차단
    FGeometryCollectionDamagePropagationData NoPropagation;
    NoPropagation.bEnabled = false;
    NoPropagation.BreakDamagePropagationFactor = 0.f;
    NoPropagation.ShockDamagePropagationFactor = 0.f;

    GeometryCollectionComponent->SetDamagePropagationData(
        NoPropagation);

    // 떨어지거나 서로 충돌하면서 다음 단계가 깨지는 것 방지
    GeometryCollectionComponent->SetEnableDamageFromCollision(false);
    GeometryCollectionComponent->SetCollisionResponseToChannel(
    ECC_Pawn,
    ECR_Ignore);

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
    
    
    UE_LOG(
        LogTemp,
        Log,
        TEXT("[%s] Destruction stage applied: %d"),
        *GetName(),
        StageIndex);

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
    CachedLocalBoundsCenter +
    StageData.LocalOffset;

    const FVector WorldCenter =
        ComponentTransform.TransformPosition(
            LocalFieldCenter);

    /**
     * BP StaticMeshComponent가 2,2,2 Scale이면
     * 파괴 반경도 그에 맞춰 증가시킨다.
     */
    const float ComponentScale =
    FMath::Max(
        ComponentTransform.GetScale3D().GetAbsMax(),
        KINDA_SMALL_NUMBER);

    const float WorldRadius =
        CachedLocalBoundsRadius *
        StageData.RadiusScale *
        ComponentScale;

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
    GetWorldTimerManager().ClearTimer(
        StrainHitTimerHandle);

    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);

    PendingTargetClusterDepth =
        FMath::Max(TargetClusterDepth, 1);

    PendingImpulseOrigin = WorldCenter;
    PendingImpulseRadius = WorldRadius;
    PendingImpulseStrength =
        StageData.ImpulseStrength;
    PendingImpulseDelay =
        StageData.ImpulseDelay;

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

    if (AppliedClusterDepth >=
        PendingTargetClusterDepth)
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

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[%s] Strain hit: %d / %d"),
        *GetName(),
        AppliedClusterDepth,
        PendingTargetClusterDepth);

    if (AppliedClusterDepth <
        PendingTargetClusterDepth)
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
    
    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);
    
    // Strain 적용이 끝난 시점. 여기서 아직 붙어 있는 조각에 불을 붙인다.
    SpawnSurfaceFires();
    
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
    
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[%s] Radial impulse fired: Strength=%.1f Radius=%.1f Origin=%s"),
        *GetName(),
        PendingImpulseStrength,
        PendingImpulseRadius,
        *PendingImpulseOrigin.ToString());
    
    GeometryCollectionComponent->AddRadialImpulse(
        PendingImpulseOrigin,
        PendingImpulseRadius,
        PendingImpulseStrength,
        ERadialImpulseFalloff::RIF_Linear,
        true); // 질량과 관계없이 속도 변화 적용

    PendingImpulseStrength = 0.f;
}

void ASFBuildingDestructionActor::SpawnSurfaceFires()
{
    if (!IsValid(SurfaceFireEffect))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BURN] SurfaceFireEffect 미지정 — BP_DestructionActor 확인"));
        return;
    }

    if (!IsValid(GeometryCollectionComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BURN] GC 컴포넌트 없음"));
        return;
    }

    ClearSurfaceFires();

    const TArray<FTransform3f>& PieceTransforms =
        GeometryCollectionComponent->GetComponentSpaceTransforms3f();

    const FBox LocalBounds = GeometryCollectionComponent->GetLocalBounds();
    const float MinZ =
            CachedRestBounds.Min.Z +
            CachedRestBounds.GetSize().Z * SurfaceFireMinHeightRatio;

    UE_LOG(LogTemp, Warning,
        TEXT("[BURN] 조각 %d개 / RestBounds Min=%s Max=%s / MinZ=%.1f"),
        PieceTransforms.Num(),
        *CachedRestBounds.Min.ToString(),
        *CachedRestBounds.Max.ToString(),
        MinZ);

    if (PieceTransforms.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BURN] 트랜스폼 배열이 비어 있음 — 물리 초기화 전"));
        return;
    }

    // 앞 5개 조각의 로컬 위치를 실제로 찍어본다.
    for (int32 i = 0; i < FMath::Min(5, PieceTransforms.Num()); ++i)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BURN]   piece[%d] local=%s"),
            i, *FVector(PieceTransforms[i].GetTranslation()).ToString());
    }

   // ── 후보 수집 ────────────────────────────────────────────
    const UGeometryCollection* Rest =
        GeometryCollectionComponent->GetRestCollection();

    if (!Rest || !Rest->GetGeometryCollection())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BURN] RestCollection 없음"));
        return;
    }

    const FGeometryCollection& Collection = *Rest->GetGeometryCollection();

    const TManagedArray<FBox>&        Boxes     = Collection.BoundingBox;
    const TManagedArray<int32>&       XfIndices = Collection.TransformIndex;
    const TManagedArray<TSet<int32>>& TChildren  = Collection.Children;

    const TManagedArray<int32>& RestParents =
        GeometryCollectionComponent->GetParentArrayRest();

    TArray<FVector> CandidateLocals;
    CandidateLocals.Reserve(Boxes.Num());

    int32 RejectedDetached = 0;
    int32 RejectedLow      = 0;

    for (int32 GeoIdx = 0; GeoIdx < Boxes.Num(); ++GeoIdx)
    {
        const int32 XfIdx = XfIndices[GeoIdx];

        if (!TChildren.IsValidIndex(XfIdx))        continue;
        if (TChildren[XfIdx].Num() != 0)           continue;   // 리프 조각만
        if (!PieceTransforms.IsValidIndex(XfIdx)) continue;

        // 클러스터에서 분리된 조각 제외. 시간에 의존하지 않는다.
        if (GeometryCollectionComponent->GetParent(XfIdx) != RestParents[XfIdx])
        {
            ++RejectedDetached;
            continue;
        }

        // 조각 바운드의 중심을 현재 트랜스폼으로 옮긴 위치.
        // 피벗이 전부 원점에 몰려 있어도 정확하다.
        const FVector LocalCenter = FVector(
            PieceTransforms[XfIdx].TransformPosition(
                FVector3f(Boxes[GeoIdx].GetCenter())));
        
        if (LocalCenter.Z < MinZ)
        {
            ++RejectedLow;
            continue;
        }

        CandidateLocals.Add(LocalCenter);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[BURN] 후보 %d개 (분리 제외 %d, 낮음 제외 %d)"),
        CandidateLocals.Num(), RejectedDetached, RejectedLow);

    if (CandidateLocals.IsEmpty())
    {
        return;
    }

    // ── 스폰 ────────────────────────────────────────────────
    const FTransform ComponentTransform =
        GeometryCollectionComponent->GetComponentTransform();

    const int32 Count = FMath::Min(
        FMath::RandRange(SurfaceFireMinCount, SurfaceFireMaxCount),
        CandidateLocals.Num());

    for (int32 i = 0; i < Count; ++i)
    {
        CandidateLocals.Swap(i, FMath::RandRange(i, CandidateLocals.Num() - 1));

        const FVector WorldLocation =
            ComponentTransform.TransformPosition(CandidateLocals[i]);
        
        UNiagaraComponent* Fire =
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                SurfaceFireEffect,
                GeometryCollectionComponent,
                NAME_None,
                WorldLocation,
                FRotator::ZeroRotator,
                EAttachLocation::KeepWorldPosition,
                false,
                true);

        DrawDebugSphere(GetWorld(), WorldLocation, 40.f, 12,
            FColor::Red, false, 10.f, 0, 2.f);

        UE_LOG(LogTemp, Warning, TEXT("[BURN] spawn[%d] world=%s -> %s"),
            i, *WorldLocation.ToString(),
            IsValid(Fire) ? TEXT("OK") : TEXT("FAIL"));

        if (IsValid(Fire))
        {
            SurfaceFires.Add(Fire);
        }
    }
}

void ASFBuildingDestructionActor::ClearSurfaceFires()
{
    for (const TObjectPtr<UNiagaraComponent>& Fire : SurfaceFires)
    {
        if (IsValid(Fire))
        {
            Fire->DestroyComponent();
        }
    }

    SurfaceFires.Reset();
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
    
    ApplyStageSettings(StageData,FinalStageIndex + 1);
    SetLifeSpan(DebrisLifetime);
}

void ASFBuildingDestructionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(StrainHitTimerHandle);
    GetWorldTimerManager().ClearTimer(ImpulseTimerHandle);

    Super::EndPlay(EndPlayReason);
}
