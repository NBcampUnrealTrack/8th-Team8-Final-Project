#include "SFNeutralSlime.h"
#include "Units/Unit/UnitAIController.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFStageBuffSubsystem.h"
#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"
#include "InGame/SFSelectINfo/SFSelectableInfo.h"
#include "UObject/ConstructorHelpers.h"

ASFNeutralSlime::ASFNeutralSlime()
{
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SlimeMesh(TEXT("/Game/ma060_Slime/SkeletalMesh/SK_ma060_Slime"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/ma060_Slime/Animations/Anim_ma060_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Walk(TEXT("/Game/ma060_Slime/Animations/Anim_ma060_Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> AttackAnim(TEXT("/Game/ma060_Slime/Animations/Anim_ma060_Attack_01"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Die(TEXT("/Game/ma060_Slime/Animations/Anim_ma060_Die"));
    GetMesh()->SetSkeletalMesh(SlimeMesh.Object);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    IdleAnimation = Idle.Object; WalkAnimation = Walk.Object; AttackAnimation = AttackAnim.Object; DieAnimation = Die.Object;
    PortraitIdleAnimation = Idle.Object;
    FindComponentByClass<UCombatTargetComponent>()->SetTeamIndex(3);
    SetAdvanceMode(EUnitAdvanceMode::HoldPosition);
    SetBlockOtherUnits(false);
}

void ASFNeutralSlime::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const float Scale = bLarge ? 1.8f : 1.f;
    GetCapsuleComponent()->SetCapsuleSize(55.f * Scale, 60.f * Scale);
    GetMesh()->SetRelativeScale3D(FVector(2.5f * Scale));
    GetMesh()->SetRelativeLocation(FVector(0, 0, -60.f * Scale));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetAnimation(IdleAnimation);
}

FSFSelectableInfo ASFNeutralSlime::GetSelectableInfo_Implementation()
{
    auto Info = Super::GetSelectableInfo_Implementation();
    Info.DisplayName = FText::FromString(bLarge ? TEXT("대형 중립 슬라임") : TEXT("소형 중립 슬라임"));
    FString Description = bLarge
        ? TEXT("공격받으면 무리가 반격합니다. 처치 시 광물 200.")
        : TEXT("공격받으면 무리가 반격합니다. 처치 시 광물 100.");
    if (Camp && Camp->bGrantClearReward)
        Description += TEXT(" 무리 전멸 시 이번 스테이지 한정 무작위 보상.");
    Info.Description = FText::FromString(Description);
    return Info;
}

void ASFNeutralSlime::BeginPlay()
{
    FUnitStatData Stats;
    Stats.MaxHealth = bLarge ? 650.f : 280.f;
    Stats.Attack = bLarge ? 28.f : 14.f;
    Stats.AttackRange = 150.f;
    Stats.AttackCooldown = 1.4f;
    Stats.MoveSpeed = 300.f;
    Stats.PopulationCost = 0;
    InitStat(Stats);
    Super::BeginPlay();
    Home = GetActorLocation();
    auto* Target = FindComponentByClass<UCombatTargetComponent>();
    Target->OnCombatDamaged.AddUniqueDynamic(this, &ASFNeutralSlime::OnDamaged);
    Target->OnTargetDestroyed.AddUniqueDynamic(this, &ASFNeutralSlime::OnKilled);
    PlayStateAnimation(IdleAnimation, true);
}

void ASFNeutralSlime::PlayStateAnimation(UAnimationAsset* Animation, bool bLoop)
{
    if (Animation && Animation != PlayingAnimation)
    {
        PlayingAnimation = Animation;
        GetMesh()->PlayAnimation(Animation, bLoop);
    }
}

void ASFNeutralSlime::Provoke(AActor* InstigatorActor)
{
    const int32 Team = USFCombatSubsystem::GetTeamOf(InstigatorActor);
    if (Team < 0 || Team == 3 || GetHealthComponent()->GetIsDead()) return;
    ProvokedTeams.Add(Team);
    Aggressor = InstigatorActor;
    SetRetaliationTarget(InstigatorActor);
}

void ASFNeutralSlime::OnDamaged(UCombatTargetComponent* Target, float Damage, AActor* InstigatorActor)
{
    if (Damage <= 0.f || !IsValid(InstigatorActor)) return;
    LastDamagingTeam = USFCombatSubsystem::GetTeamOf(InstigatorActor);
    if (Camp) Camp->Provoke(InstigatorActor);
    else Provoke(InstigatorActor);
}

void ASFNeutralSlime::OnKilled(UCombatTargetComponent* Target)
{
    if (bRewardPaid) return;
    bRewardPaid = true;
    PlayStateAnimation(DieAnimation, false);
    if (auto* GS = GetWorld()->GetGameState<ASFGameState>())
        if (LastDamagingTeam >= 0 && LastDamagingTeam != 3 && GS->GetTeamState(LastDamagingTeam))
            GS->AddResource(LastDamagingTeam, EResourceType::Mineral, bLarge ? 200 : 100);
    if (Camp) Camp->MemberKilled(this, LastDamagingTeam);
}

void ASFNeutralSlime::Tick(float DeltaSeconds)
{
    if (GetHealthComponent()->GetIsDead()) return;
    const auto* Target = USFCombatSubsystem::FindTargetComponent(Aggressor.Get());
    if (!Target || !Target->IsAlive() || FVector::DistSquared2D(GetActorLocation(), Home) > FMath::Square(1400.f)
        || FVector::DistSquared2D(Aggressor->GetActorLocation(), Home) > FMath::Square(1600.f))
    {
        Aggressor.Reset();
        SetRetaliationTarget(nullptr);
        CombatState = EUnitCombatState::Advancing;
        if (auto* AI = Cast<AUnitAIController>(GetController()))
        {
            if (FVector::DistSquared2D(GetActorLocation(), Home) > FMath::Square(45.f))
                AI->RequestMoveToLocation(Home, 20.f);
            else { AI->ClearMoveRequest(); ProvokedTeams.Reset(); }
        }
        PlayStateAnimation(GetGroundSpeed() > 5.f ? WalkAnimation : IdleAnimation, true);
        return;
    }
    SetRetaliationTarget(Aggressor.Get());
    Super::Tick(DeltaSeconds);
    PlayStateAnimation(CombatState == EUnitCombatState::Attacking ? AttackAnimation : GetGroundSpeed() > 5.f ? WalkAnimation : IdleAnimation, true);
}

ASFNeutralSlimeCamp::ASFNeutralSlimeCamp()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASFNeutralSlimeCamp::Provoke(AActor* InstigatorActor)
{
    for (ASFNeutralSlime* Member : Members) if (IsValid(Member)) Member->Provoke(InstigatorActor);
}

void ASFNeutralSlimeCamp::MemberKilled(ASFNeutralSlime* Member, int32 Team)
{
    if (bCleared || !Members.Contains(Member)) return;
    KilledMembers.Add(Member);
    // Destroying/unloading actors is not a kill and must never grant a reward.
    if (Members.IsEmpty() || KilledMembers.Num() != Members.Num()) return;
    bCleared = true;
    if (bGrantClearReward)
        GetWorld()->GetSubsystem<USFStageBuffSubsystem>()->GrantRandomReward(Team);
}
