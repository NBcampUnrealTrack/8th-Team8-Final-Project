#include "Buildings/GC/SFBuildingDestructionActor.h"

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
        ECC_Visibility,
        ECR_Ignore);

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

void ASFBuildingDestructionActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        StrainHitTimerHandle);

    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);

    Super::EndPlay(EndPlayReason);
}