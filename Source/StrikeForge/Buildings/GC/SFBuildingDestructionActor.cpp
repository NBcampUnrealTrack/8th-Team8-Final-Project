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

    // 체력 50% 중간 파괴 한 단계
    DamageStages.SetNum(1);

    DamageStages[0].Strain = 950000.f;
    DamageStages[0].Radius = 1000.f;
    DamageStages[0].LocalOffset = FVector(0.f, 0.f, 300.f);
    DamageStages[0].ImpulseStrength = 0.f;
    DamageStages[0].ImpulseDelay = 0.04f;

    // 체력 0 최종 파괴
    FinalStage.Strain = 1500000.f;
    FinalStage.Radius = 1200.f;
    FinalStage.LocalOffset = FVector(0.f, 0.f, 250.f);
    FinalStage.ImpulseStrength = 450.f;
    FinalStage.ImpulseDelay = 0.04f;
}

bool ASFBuildingDestructionActor::InitializeDestruction(
    const UGeometryCollection* InCollection,
    const FTransform& InVisualTransform)
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
    
    /**
     * StaticMeshComponent의 월드 Transform을 그대로 사용한다.
     * BP에서 설정한 상대 Scale도 이 Transform에 포함된다.
     */
    SetActorTransform(
        InVisualTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    
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

void ASFBuildingDestructionActor::ApplyDamageStage(
    const int32 StageIndex)
{
    if (!bInitialized || bFinalDestructionApplied)
    {
        return;
    }

    if (!DamageStages.IsValidIndex(StageIndex))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[%s] Invalid destruction stage: %d"),
            *GetName(),
            StageIndex);

        return;
    }

    // 동일 단계 재실행 및 회복 방향 역실행 방지
    if (StageIndex <= AppliedStageIndex)
    {
        return;
    }

    AppliedStageIndex = StageIndex;

    ApplyStageSettings(DamageStages[StageIndex]);

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[%s] Destruction stage applied: %d"),
        *GetName(),
        StageIndex);
}

void ASFBuildingDestructionActor::ApplyFinalDestruction()
{
    if (!bInitialized || bFinalDestructionApplied)
    {
        return;
    }

    bFinalDestructionApplied = true;
    
    UE_LOG(
            LogTemp,
            Warning,
            TEXT("[%s] Final destruction applied"),
            *GetName());
    
    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);

    ApplyStageSettings(FinalStage);
    RemoveAfter(DebrisLifetime);
}

void ASFBuildingDestructionActor::ApplyStageSettings(
    const FSFBuildingDestructionStage& StageSettings)
{
    if (!IsValid(GeometryCollectionComponent) ||
        !IsValid(StrainField))
    {
        return;
    }

    const FTransform& ComponentTransform =
        GeometryCollectionComponent->GetComponentTransform();

    const FVector WorldCenter =
        ComponentTransform.TransformPosition(
            StageSettings.LocalOffset);

    /**
     * BP StaticMeshComponent가 2,2,2 Scale이면
     * 파괴 반경도 그에 맞춰 증가시킨다.
     */
    const float ComponentScale =
        FMath::Max(
            ComponentTransform.GetScale3D().GetAbsMax(),
            KINDA_SMALL_NUMBER);

    const float WorldRadius =
        StageSettings.Radius * ComponentScale;

    /**
     * Falloff None:
     * 반경 안의 파편에는 지정한 Strain이 그대로 적용된다.
     * 초기 단계 검증 시 가장 예측하기 쉽다.
     */
    StrainField->SetRadialFalloff(
        StageSettings.Strain,
        0.f,
        1.f,
        0.f,
        WorldRadius,
        WorldCenter,
        EFieldFalloffType::Field_FallOff_None);

    /**
     * 이 GeometryCollectionComponent의 PhysicsProxy에만 전달된다.
     * 주변 건물 GC에는 적용되지 않는다.
     */
    GeometryCollectionComponent->ApplyPhysicsField(
        true,
        EGeometryCollectionPhysicsTypeEnum::
            Chaos_ExternalClusterStrain,
        nullptr,
        StrainField);

    if (StageSettings.ImpulseStrength <= 0.f)
    {
        return;
    }

    PendingImpulseOrigin = WorldCenter;
    PendingImpulseRadius = WorldRadius;
    PendingImpulseStrength =
        StageSettings.ImpulseStrength;

    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);

    if (StageSettings.ImpulseDelay <= 0.f)
    {
        ApplyPendingImpulse();
        return;
    }

    /**
     * Strain으로 클러스터가 먼저 분리된 다음,
     * 새로 활성화된 파편에 Impulse를 적용한다.
     *
     * 초기화 순서를 억지로 맞추는 타이머가 아니라
     * 두 물리 연출을 순차 실행하기 위한 짧은 지연이다.
     */
    GetWorldTimerManager().SetTimer(
        ImpulseTimerHandle,
        this,
        &ThisClass::ApplyPendingImpulse,
        StageSettings.ImpulseDelay,
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

    GeometryCollectionComponent->AddRadialImpulse(
        PendingImpulseOrigin,
        PendingImpulseRadius,
        PendingImpulseStrength,
        ERadialImpulseFalloff::RIF_Linear,
        true); // Velocity Change

    PendingImpulseStrength = 0.f;
}

void ASFBuildingDestructionActor::RemoveAfter(
    const float InSeconds)
{
    if (InSeconds <= 0.f)
    {
        Destroy();
        return;
    }

    SetLifeSpan(InSeconds);
}

void ASFBuildingDestructionActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        ImpulseTimerHandle);

    Super::EndPlay(EndPlayReason);
}